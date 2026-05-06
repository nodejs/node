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

// V8 fast-call's hard cap is 8 C-side args. With HasReceiver=kNo there's no
// implicit receiver in the count, so user functions can have up to 8 args.
TEST(FFIFastCallEligibility, EightArgsAccepted) {
  std::vector<ffi_type*> args(8, &ffi_type_sint32);
  std::vector<std::string> names(8, "i32");
  auto fn = MakeFn(&ffi_type_void, "void", std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_TRUE(IsFastCallEligible(fn, &reason));
}

TEST(FFIFastCallEligibility, NineArgsRejected) {
  std::vector<ffi_type*> args(9, &ffi_type_sint32);
  std::vector<std::string> names(9, "i32");
  auto fn = MakeFn(&ffi_type_void, "void", std::move(args), std::move(names));
  const char* reason = nullptr;
  EXPECT_FALSE(IsFastCallEligible(fn, &reason));
  ASSERT_NE(reason, nullptr);
  EXPECT_NE(std::string(reason).find("argument count"), std::string::npos);
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

#endif  // HAVE_FFI_FASTCALL
