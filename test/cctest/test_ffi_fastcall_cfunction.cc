#include "gtest/gtest.h"

#ifdef HAVE_FFI_FASTCALL

#include <string>
#include <vector>

#include "ffi/fastcall/cfunction_info.h"
#include "node_ffi.h"

using node::ffi::FFIFunction;
using node::ffi::fastcall::ArgClass;
using node::ffi::fastcall::BuildCFunctionInfo;
using node::ffi::fastcall::ResultClass;

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
  // Receiver + 2 args = 3 CTypeInfo entries.
  EXPECT_EQ(bundle.info->ArgumentCount(), 3u);
  EXPECT_EQ(bundle.arg_classes.size(), 2u);
  EXPECT_EQ(bundle.arg_classes[0], ArgClass::kGP);
  EXPECT_EQ(bundle.arg_classes[1], ArgClass::kGP);
  EXPECT_EQ(bundle.result_class, ResultClass::kGP);
}

TEST(FFIFastCallCFunction, FloatSignature) {
  auto fn = MakeFn(&ffi_type_double, "double",
                   {&ffi_type_float, &ffi_type_double},
                   {"float", "double"});
  auto bundle = BuildCFunctionInfo(fn);
  EXPECT_EQ(bundle.arg_classes[0], ArgClass::kFP);
  EXPECT_EQ(bundle.arg_classes[1], ArgClass::kFP);
  EXPECT_EQ(bundle.result_class, ResultClass::kFP);
}

TEST(FFIFastCallCFunction, VoidReturn) {
  auto fn = MakeFn(&ffi_type_void, "void",
                   {&ffi_type_sint32}, {"i32"});
  auto bundle = BuildCFunctionInfo(fn);
  EXPECT_EQ(bundle.result_class, ResultClass::kVoid);
}

TEST(FFIFastCallCFunction, PointerSignature) {
  auto fn = MakeFn(&ffi_type_pointer, "pointer",
                   {&ffi_type_pointer}, {"pointer"});
  auto bundle = BuildCFunctionInfo(fn);
  EXPECT_EQ(bundle.arg_classes[0], ArgClass::kGP);
  EXPECT_EQ(bundle.result_class, ResultClass::kGP);
}

TEST(FFIFastCallCFunction, MoveCleanupSafe) {
  auto fn = MakeFn(&ffi_type_sint32, "i32",
                   {&ffi_type_sint32}, {"i32"});
  auto a = BuildCFunctionInfo(fn);
  // Move into a new bundle and let it drop. The moved-from bundle's
  // destructor must not double-free.
  auto b = std::move(a);
  EXPECT_EQ(a.info, nullptr);
  EXPECT_EQ(a.arg_types, nullptr);
  // b's destructor runs at end of scope; no crash expected.
}

TEST(FFIFastCallCFunction, MoveAssignmentSelfSafe) {
  auto fn = MakeFn(&ffi_type_sint32, "i32",
                   {&ffi_type_sint32}, {"i32"});
  auto bundle = BuildCFunctionInfo(fn);
  ASSERT_NE(bundle.info, nullptr);
  ASSERT_NE(bundle.arg_types, nullptr);
  ASSERT_EQ(bundle.arg_classes.size(), 1u);
  ASSERT_EQ(bundle.result_class, node::ffi::fastcall::ResultClass::kGP);

  // Self-move-assign must be a no-op (or at least not double-free).
  // The self-assignment guard should keep all four bundle fields valid.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  bundle = std::move(bundle);
#pragma GCC diagnostic pop
  EXPECT_NE(bundle.info, nullptr);
  EXPECT_NE(bundle.arg_types, nullptr);
  EXPECT_EQ(bundle.arg_classes.size(), 1u);
  EXPECT_EQ(bundle.result_class, node::ffi::fastcall::ResultClass::kGP);
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
  // bundle_a now holds what bundle_b had; bundle_b is nulled out.
  EXPECT_EQ(bundle_b.info, nullptr);
  EXPECT_NE(bundle_a.info, nullptr);
  EXPECT_NE(bundle_a.info, old_a_info);
}

#endif  // HAVE_FFI_FASTCALL
