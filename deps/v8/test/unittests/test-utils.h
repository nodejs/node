// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_TEST_UTILS_H_
#define V8_UNITTESTS_TEST_UTILS_H_

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/utils/random-number-generator.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "testing/gtest-support.h"

namespace v8 {

class ArrayBufferAllocator;


class TestWithIsolate : public virtual ::testing::Test {
 public:
  TestWithIsolate();
  virtual ~TestWithIsolate();

  Isolate* isolate() const { return isolate_; }

  static void SetUpTestCase();
  static void TearDownTestCase();

 private:
  static v8::ArrayBuffer::Allocator* array_buffer_allocator_;
  static Isolate* isolate_;
  Isolate::Scope isolate_scope_;
  HandleScope handle_scope_;

  DISALLOW_COPY_AND_ASSIGN(TestWithIsolate);
};


class TestWithContext : public virtual TestWithIsolate {
 public:
  TestWithContext();
  virtual ~TestWithContext();

  const Local<Context>& context() const { return context_; }

  v8::internal::Isolate* i_isolate() const {
    return reinterpret_cast<v8::internal::Isolate*>(isolate());
  }

 private:
  Local<Context> context_;
  Context::Scope context_scope_;

  DISALLOW_COPY_AND_ASSIGN(TestWithContext);
};


namespace base {

class TestWithRandomNumberGenerator : public ::testing::Test {
 public:
  TestWithRandomNumberGenerator();
  virtual ~TestWithRandomNumberGenerator();

  RandomNumberGenerator* rng() { return &rng_; }

 private:
  RandomNumberGenerator rng_;

  DISALLOW_COPY_AND_ASSIGN(TestWithRandomNumberGenerator);
};

}  // namespace base


namespace internal {

// Forward declarations.
class Factory;


class TestWithIsolate : public virtual ::v8::TestWithIsolate {
 public:
  TestWithIsolate() {}
  virtual ~TestWithIsolate();

  Factory* factory() const;
  Isolate* isolate() const {
    return reinterpret_cast<Isolate*>(::v8::TestWithIsolate::isolate());
  }
  base::RandomNumberGenerator* random_number_generator() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWithIsolate);
};


class TestWithZone : public virtual ::testing::Test {
 public:
  TestWithZone() : zone_(&allocator_) {}
  virtual ~TestWithZone();

  Zone* zone() { return &zone_; }

 private:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;

  DISALLOW_COPY_AND_ASSIGN(TestWithZone);
};


class TestWithIsolateAndZone : public virtual TestWithIsolate {
 public:
  TestWithIsolateAndZone() : zone_(&allocator_) {}
  virtual ~TestWithIsolateAndZone();

  Zone* zone() { return &zone_; }

 private:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;

  DISALLOW_COPY_AND_ASSIGN(TestWithIsolateAndZone);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_TEST_UTILS_H_
