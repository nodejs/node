// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_UNITTESTS_COMPILER_UNITTESTS_H_
#define V8_COMPILER_UNITTESTS_COMPILER_UNITTESTS_H_

#include "include/v8.h"
#include "src/zone.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace compiler {

// The COMPILER_TEST(Case, Name) macro works just like
// TEST(Case, Name), except that the test is disabled
// if the platform is not a supported TurboFan target.
#if V8_TURBOFAN_TARGET
#define COMPILER_TEST(Case, Name) TEST(Case, Name)
#else
#define COMPILER_TEST(Case, Name) TEST(Case, DISABLED_##Name)
#endif


// The COMPILER_TEST_F(Case, Name) macro works just like
// TEST_F(Case, Name), except that the test is disabled
// if the platform is not a supported TurboFan target.
#if V8_TURBOFAN_TARGET
#define COMPILER_TEST_F(Case, Name) TEST_F(Case, Name)
#else
#define COMPILER_TEST_F(Case, Name) TEST_F(Case, DISABLED_##Name)
#endif


class CompilerTest : public ::testing::Test {
 public:
  CompilerTest();
  virtual ~CompilerTest();

  Isolate* isolate() const { return reinterpret_cast<Isolate*>(isolate_); }
  Zone* zone() { return &zone_; }

 protected:
  static void SetUpTestCase();
  static void TearDownTestCase();

 private:
  static v8::Isolate* isolate_;
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;
  v8::Context::Scope context_scope_;
  Zone zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_UNITTESTS_COMPILER_UNITTESTS_H_
