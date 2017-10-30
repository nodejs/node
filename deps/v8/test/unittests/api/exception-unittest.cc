// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "src/flags.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using APIExceptionTest = TestWithIsolate;

class ScopedExposeGc {
 public:
  ScopedExposeGc() : was_exposed_(i::FLAG_expose_gc) {
    i::FLAG_expose_gc = true;
  }
  ~ScopedExposeGc() { i::FLAG_expose_gc = was_exposed_; }

 private:
  const bool was_exposed_;
};

TEST_F(APIExceptionTest, ExceptionMessageDoesNotKeepContextAlive) {
  ScopedExposeGc expose_gc;
  Persistent<Context> weak_context;
  {
    HandleScope handle_scope(isolate());
    Local<Context> context = Context::New(isolate());
    weak_context.Reset(isolate(), context);
    weak_context.SetWeak();

    Context::Scope context_scope(context);
    TryCatch try_catch(isolate());
    isolate()->ThrowException(Undefined(isolate()));
  }
  isolate()->RequestGarbageCollectionForTesting(
      Isolate::kFullGarbageCollection);
  EXPECT_TRUE(weak_context.IsEmpty());
}

}  // namespace
}  // namespace v8
