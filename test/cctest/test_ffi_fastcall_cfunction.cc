#include "gtest/gtest.h"

#ifdef HAVE_FFI_FASTCALL

#include <string>
#include <vector>

#include "ffi/fastcall/cfunction_info.h"
#include "node_ffi.h"

using node::ffi::FFIFunction;
using node::ffi::fastcall::BuildCFunctionInfo;

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

TEST(FFIFastCallCFunction, NumericSignature) {
  auto fn = MakeFn(&ffi_type_sint32, "i32",
                   {&ffi_type_sint32, &ffi_type_sint32},
                   {"i32", "i32"});
  auto bundle = BuildCFunctionInfo(fn);
  // HasReceiver=kNo: ArgumentCount counts user args only (no leading
  // v8::Value receiver slot).
  EXPECT_EQ(bundle.info->ArgumentCount(), 2u);
  EXPECT_FALSE(bundle.info->HasReceiverArg());
}

TEST(FFIFastCallCFunction, FloatSignature) {
  auto fn = MakeFn(&ffi_type_double, "double",
                   {&ffi_type_float, &ffi_type_double},
                   {"float", "double"});
  auto bundle = BuildCFunctionInfo(fn);
  EXPECT_EQ(bundle.info->ArgumentCount(), 2u);
}

TEST(FFIFastCallCFunction, VoidReturn) {
  auto fn = MakeFn(&ffi_type_void, "void",
                   {&ffi_type_sint32}, {"i32"});
  auto bundle = BuildCFunctionInfo(fn);
  EXPECT_EQ(bundle.info->ArgumentCount(), 1u);
}

TEST(FFIFastCallCFunction, PointerSignature) {
  auto fn = MakeFn(&ffi_type_pointer, "pointer",
                   {&ffi_type_pointer}, {"pointer"});
  auto bundle = BuildCFunctionInfo(fn);
  EXPECT_EQ(bundle.info->ArgumentCount(), 1u);
}

TEST(FFIFastCallCFunction, MoveCleanupSafe) {
  auto fn = MakeFn(&ffi_type_sint32, "i32",
                   {&ffi_type_sint32}, {"i32"});
  auto a = BuildCFunctionInfo(fn);
  auto b = std::move(a);
  EXPECT_EQ(a.info, nullptr);
  EXPECT_EQ(a.arg_types, nullptr);
}

TEST(FFIFastCallCFunction, MoveAssignmentSelfSafe) {
  auto fn = MakeFn(&ffi_type_sint32, "i32",
                   {&ffi_type_sint32}, {"i32"});
  auto bundle = BuildCFunctionInfo(fn);
  ASSERT_NE(bundle.info, nullptr);
  ASSERT_NE(bundle.arg_types, nullptr);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  bundle = std::move(bundle);
#pragma GCC diagnostic pop
  EXPECT_NE(bundle.info, nullptr);
  EXPECT_NE(bundle.arg_types, nullptr);
}

TEST(FFIFastCallCFunction, MoveAssignmentReplaces) {
  auto fn_a = MakeFn(&ffi_type_sint32, "i32",
                     {&ffi_type_sint32}, {"i32"});
  auto fn_b = MakeFn(&ffi_type_double, "double",
                     {&ffi_type_double}, {"double"});
  auto bundle_a = BuildCFunctionInfo(fn_a);
  auto bundle_b = BuildCFunctionInfo(fn_b);
  const void* old_a_info = bundle_a.info;
  bundle_a = std::move(bundle_b);
  EXPECT_EQ(bundle_b.info, nullptr);
  EXPECT_NE(bundle_a.info, nullptr);
  EXPECT_NE(bundle_a.info, old_a_info);
}

#endif  // HAVE_FFI_FASTCALL
