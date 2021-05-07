// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-cppgc.h"
#include "src/heap/cppgc/visitor.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using TracedReferenceTest = TestWithIsolate;

TEST_F(TracedReferenceTest, ResetFromLocal) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  v8::TracedReference<v8::Object> ref;
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    EXPECT_TRUE(ref.IsEmpty());
    EXPECT_NE(ref, local);
    ref.Reset(v8_isolate(), local);
    EXPECT_FALSE(ref.IsEmpty());
    EXPECT_EQ(ref, local);
  }
}

TEST_F(TracedReferenceTest, ConstructFromLocal) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref(v8_isolate(), local);
    EXPECT_FALSE(ref.IsEmpty());
    EXPECT_EQ(ref, local);
  }
}

TEST_F(TracedReferenceTest, Reset) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref(v8_isolate(), local);
    EXPECT_FALSE(ref.IsEmpty());
    EXPECT_EQ(ref, local);
    ref.Reset();
    EXPECT_TRUE(ref.IsEmpty());
    EXPECT_NE(ref, local);
  }
}

TEST_F(TracedReferenceTest, Copy) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref(v8_isolate(), local);
    v8::TracedReference<v8::Object> ref_copy1(ref);
    v8::TracedReference<v8::Object> ref_copy2 = ref;
    EXPECT_EQ(ref, local);
    EXPECT_EQ(ref_copy1, local);
    EXPECT_EQ(ref_copy2, local);
  }
}

TEST_F(TracedReferenceTest, CopyHeterogenous) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref(v8_isolate(), local);
    v8::TracedReference<v8::Value> ref_copy1(ref);
    v8::TracedReference<v8::Value> ref_copy2 = ref;
    EXPECT_EQ(ref, local);
    EXPECT_EQ(ref_copy1, local);
    EXPECT_EQ(ref_copy2, local);
  }
}

TEST_F(TracedReferenceTest, Move) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref(v8_isolate(), local);
    v8::TracedReference<v8::Object> ref_moved1(std::move(ref));
    v8::TracedReference<v8::Object> ref_moved2 = std::move(ref_moved1);
    EXPECT_TRUE(ref.IsEmpty());
    EXPECT_TRUE(ref_moved1.IsEmpty());
    EXPECT_EQ(ref_moved2, local);
  }
}

TEST_F(TracedReferenceTest, MoveHeterogenous) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref1(v8_isolate(), local);
    v8::TracedReference<v8::Value> ref_moved1(std::move(ref1));
    v8::TracedReference<v8::Object> ref2(v8_isolate(), local);
    v8::TracedReference<v8::Object> ref_moved2 = std::move(ref2);
    EXPECT_TRUE(ref1.IsEmpty());
    EXPECT_EQ(ref_moved1, local);
    EXPECT_TRUE(ref2.IsEmpty());
    EXPECT_EQ(ref_moved2, local);
  }
}

TEST_F(TracedReferenceTest, Equality) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local1 =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref1(v8_isolate(), local1);
    v8::TracedReference<v8::Object> ref2(v8_isolate(), local1);
    EXPECT_EQ(ref1, ref2);
    EXPECT_EQ(ref2, ref1);
    v8::Local<v8::Object> local2 =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref3(v8_isolate(), local2);
    EXPECT_NE(ref2, ref3);
    EXPECT_NE(ref3, ref2);
  }
}

TEST_F(TracedReferenceTest, EqualityHeterogenous) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local1 =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref1(v8_isolate(), local1);
    v8::TracedReference<v8::Value> ref2(v8_isolate(), local1);
    EXPECT_EQ(ref1, ref2);
    EXPECT_EQ(ref2, ref1);
    v8::Local<v8::Object> local2 =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref3(v8_isolate(), local2);
    EXPECT_NE(ref2, ref3);
    EXPECT_NE(ref3, ref2);
  }
}

namespace {

// Must be used on stack.
class JSVisitorForTesting final : public JSVisitor {
 public:
  explicit JSVisitorForTesting(v8::Local<v8::Object> expected_object)
      : JSVisitor(cppgc::internal::VisitorFactory::CreateKey()),
        expected_object_(expected_object) {}

  void Visit(const TracedReferenceBase& ref) final {
    EXPECT_EQ(ref, expected_object_);
    visit_count_++;
  }

  size_t visit_count() const { return visit_count_; }

 private:
  v8::Local<v8::Object> expected_object_;
  size_t visit_count_ = 0;
};

}  // namespace

TEST_F(TracedReferenceTest, TracedReferenceTrace) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> js_member(v8_isolate(), local);
    JSVisitorForTesting visitor(local);
    // Cast to cppgc::Visitor to ensure that we dispatch through the base
    // visitor and use traits.
    static_cast<cppgc::Visitor&>(visitor).Trace(js_member);
    EXPECT_EQ(1u, visitor.visit_count());
  }
}

}  // namespace internal
}  // namespace v8
