// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_TEST_UTILS_H_
#define V8_UNITTESTS_TEST_UTILS_H_

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/utils/random-number-generator.h"
#include "src/zone.h"
#include "testing/gtest-support.h"

namespace v8 {

std::ostream& operator<<(std::ostream&, ExternalArrayType);


class TestWithIsolate : public ::testing::Test {
 public:
  TestWithIsolate();
  virtual ~TestWithIsolate();

  Isolate* isolate() const { return isolate_; }

  static void SetUpTestCase();
  static void TearDownTestCase();

 private:
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

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWithIsolate);
};


class TestWithZone : public TestWithIsolate {
 public:
  TestWithZone() : zone_(isolate()) {}
  virtual ~TestWithZone();

  Zone* zone() { return &zone_; }

 private:
  Zone zone_;

  DISALLOW_COPY_AND_ASSIGN(TestWithZone);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_TEST_UTILS_H_
