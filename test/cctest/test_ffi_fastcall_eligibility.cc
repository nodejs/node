#include "gtest/gtest.h"

#ifdef HAVE_FFI_FASTCALL

#include <string>
#include <vector>

#include "ffi.h"
#include "ffi/types.h"
#include "node_ffi.h"

using node::ffi::FFIFunction;
using node::ffi::IsFastCallEligible;

namespace {
FFIFunction MakeFn(ffi_type* ret,
                   std::string ret_name,
                   std::vector<ffi_type*> args,
                   std::vector<std::string> arg_names) {
  FFIFunction fn{};
  fn.return_type = ret;
  fn.return_type_name = std::move(ret_name);
  fn.args = std::move(args);
  fn.arg_type_names = std::move(arg_names);
  return fn;
}
}  // namespace

TEST(FFIFastCallEligibility, AllNumeric) {
  auto fn = MakeFn(&ffi_type_sint32, "i32",
                   {&ffi_type_sint32, &ffi_type_sint32},
                   {"i32", "i32"});
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, VoidReturnIsOk) {
  auto fn = MakeFn(&ffi_type_void, "void",
                   {&ffi_type_sint32}, {"i32"});
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, FloatsOk) {
  auto fn = MakeFn(&ffi_type_double, "double",
                   {&ffi_type_float, &ffi_type_double},
                   {"float", "double"});
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, PointerOk) {
  auto fn = MakeFn(&ffi_type_pointer, "pointer",
                   {&ffi_type_pointer}, {"pointer"});
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, FunctionTypeRejected) {
  auto fn = MakeFn(&ffi_type_void, "void",
                   {&ffi_type_pointer}, {"function"});
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
  ASSERT_NE(reason, nullptr);
  EXPECT_NE(std::string(reason).find("function"), std::string::npos);
}

TEST(FFIFastCallEligibility, FunctionReturnRejected) {
  auto fn = MakeFn(&ffi_type_pointer, "function",
                   {&ffi_type_sint32}, {"i32"});
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, TooManyGPArgsRejected) {
  // 8 GP args is one over AArch64's cap of 7 (the largest GP cap of any
  // supported platform), so this is rejected on every supported emitter.
  std::vector<ffi_type*> args(8, &ffi_type_sint32);
  std::vector<std::string> names(8, "i32");
  auto fn = MakeFn(&ffi_type_void, "void", std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
  ASSERT_NE(reason, nullptr);
  // On non-Win64 platforms the reason mentions "GP arg"; on Win64 the combined
  // gp+fp cap fires with "Win64 cap". Accept either.
  const std::string r(reason);
  EXPECT_TRUE(r.find("GP arg") != std::string::npos ||
              r.find("Win64 cap") != std::string::npos);
}

TEST(FFIFastCallEligibility, TooManyFPArgsRejected) {
  // 9 FP args is one over the FP cap of 8.
  std::vector<ffi_type*> args(9, &ffi_type_double);
  std::vector<std::string> names(9, "double");
  auto fn = MakeFn(&ffi_type_void, "void", std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
  ASSERT_NE(reason, nullptr);
  // On non-Win64 the reason mentions "FP arg"; on Win64 the combined cap fires.
  const std::string r(reason);
  EXPECT_TRUE(r.find("FP arg") != std::string::npos ||
              r.find("Win64 cap") != std::string::npos);
}

TEST(FFIFastCallEligibility, VoidArgRejected) {
  auto fn = MakeFn(&ffi_type_sint32, "i32",
                   {&ffi_type_void}, {"void"});
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
  ASSERT_NE(reason, nullptr);
  EXPECT_NE(std::string(reason).find("void"), std::string::npos);
}

TEST(FFIFastCallEligibility, NullOutReasonOk) {
  auto fn = MakeFn(&ffi_type_sint32, "i32",
                   {&ffi_type_sint32}, {"i32"});
  EXPECT_TRUE(IsFastCallEligible(fn, nullptr));
}

// Per-platform GP cap boundary tests.

#if defined(__aarch64__) || defined(_M_ARM64)
// AArch64 allows up to 7 GP args — the largest GP cap of any supported
// platform. Verify that 7 is accepted and 8 is rejected.
TEST(FFIFastCallEligibility, AArch64GPCapBoundary) {
  std::vector<ffi_type*> args(7, &ffi_type_sint32);
  std::vector<std::string> names(7, "i32");
  auto fn = MakeFn(&ffi_type_void, "void", std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}
#endif  // AArch64

#if defined(__x86_64__) && \
    (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__))
// SysV (Linux/macOS/FreeBSD x86_64) allows up to 6 GP args. Verify that 6
// is accepted and 7 is rejected.
TEST(FFIFastCallEligibility, SysVGPCapBoundary) {
  std::vector<ffi_type*> args(6, &ffi_type_sint32);
  std::vector<std::string> names(6, "i32");
  auto fn = MakeFn(&ffi_type_void, "void", std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, SysVGPCapExceeded) {
  std::vector<ffi_type*> args(7, &ffi_type_sint32);
  std::vector<std::string> names(7, "i32");
  auto fn = MakeFn(&ffi_type_void, "void", std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
}
#endif  // SysV x86_64

#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
// Win64 allows at most 3 combined GP+FP args. Verify the boundary.
TEST(FFIFastCallEligibility, Win64CombinedCapBoundary) {
  std::vector<ffi_type*> args(3, &ffi_type_sint32);
  std::vector<std::string> names(3, "i32");
  auto fn = MakeFn(&ffi_type_void, "void", std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, Win64CombinedCapExceeded) {
  std::vector<ffi_type*> args(4, &ffi_type_sint32);
  std::vector<std::string> names(4, "i32");
  auto fn = MakeFn(&ffi_type_void, "void", std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
}
#endif  // Win64

TEST(FFIFastCallEligibility, StringTypeNameOk) {
  auto fn = MakeFn(&ffi_type_pointer, "pointer",
                   {&ffi_type_pointer}, {"string"});
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, BufferTypeNameOk) {
  auto fn = MakeFn(&ffi_type_pointer, "pointer",
                   {&ffi_type_pointer}, {"buffer"});
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, ArraybufferTypeNameOk) {
  auto fn = MakeFn(&ffi_type_pointer, "pointer",
                   {&ffi_type_pointer}, {"arraybuffer"});
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

#if defined(__arm__)
TEST(FFIFastCallEligibility, ARM32GPCapBoundary) {
  std::vector<ffi_type*> args(3, &ffi_type_sint32);
  std::vector<std::string> names(3, "i32");
  auto fn = MakeFn(&ffi_type_void, "void",
                   std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, ARM32GPCapExceeded) {
  std::vector<ffi_type*> args(4, &ffi_type_sint32);
  std::vector<std::string> names(4, "i32");
  auto fn = MakeFn(&ffi_type_void, "void",
                   std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, ARM32I64ArgRejected) {
  auto fn = MakeFn(&ffi_type_void, "void",
                   {&ffi_type_sint64}, {"i64"});
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
  ASSERT_NE(reason, nullptr);
  EXPECT_NE(std::string(reason).find("aarch32"), std::string::npos);
}

TEST(FFIFastCallEligibility, ARM32I64ReturnRejected) {
  auto fn = MakeFn(&ffi_type_sint64, "i64",
                   {&ffi_type_sint32}, {"i32"});
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
  ASSERT_NE(reason, nullptr);
  EXPECT_NE(std::string(reason).find("aarch32"), std::string::npos);
}
#endif  // __arm__

#endif  // HAVE_FFI_FASTCALL
