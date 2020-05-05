// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/finalizer-trait.h"
#include <type_traits>
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

// Trivially destructible types.
class TypeWithoutDestructor final {};
class TypeWithPrimitive final {
 public:
  int foo = 0;
};

class InvokeCounter {
 public:
  static size_t kCallcount;
  static void Reset() { kCallcount = 0; }
  static void Invoke() { kCallcount++; }
};

size_t InvokeCounter::kCallcount = 0;

// Regular C++ use cases.

class TypeWithDestructor final : public InvokeCounter {
 public:
  ~TypeWithDestructor() { Invoke(); }
};

class TypeWithVirtualDestructorBase {
 public:
  virtual ~TypeWithVirtualDestructorBase() = default;
};

class TypeWithVirtualDestructorChild final
    : public TypeWithVirtualDestructorBase,
      public InvokeCounter {
 public:
  ~TypeWithVirtualDestructorChild() final { Invoke(); }
};

// Manual dispatch to avoid vtables.

class TypeWithCustomFinalizationMethod final : public InvokeCounter {
 public:
  void FinalizeGarbageCollectedObject() { Invoke(); }
};

class TypeWithCustomFinalizationMethodAtBase {
 public:
  void FinalizeGarbageCollectedObject();
};

class TypeWithCustomFinalizationMethodAtBaseChild
    : public TypeWithCustomFinalizationMethodAtBase,
      public InvokeCounter {
 public:
  ~TypeWithCustomFinalizationMethodAtBaseChild() { Invoke(); }
};

void TypeWithCustomFinalizationMethodAtBase::FinalizeGarbageCollectedObject() {
  // The test knows that base is only inherited by a single child. In practice
  // users can maintain a map of valid types in already existing storage.
  static_cast<TypeWithCustomFinalizationMethodAtBaseChild*>(this)
      ->~TypeWithCustomFinalizationMethodAtBaseChild();
}

template <typename Type>
void ExpectFinalizerIsInvoked(Type* object) {
  InvokeCounter::Reset();
  EXPECT_NE(nullptr, FinalizerTrait<Type>::kCallback);
  FinalizerTrait<Type>::kCallback(object);
  EXPECT_EQ(1u, InvokeCounter::kCallcount);
  operator delete(object);
}

}  // namespace

TEST(FinalizerTrait, TypeWithoutDestructorHasNoFinalizer) {
  static_assert(std::is_trivially_destructible<TypeWithoutDestructor>::value,
                "trivially destructible");
  EXPECT_EQ(nullptr, FinalizerTrait<TypeWithoutDestructor>::kCallback);
}

TEST(FinalizerTrait, TypeWithPrimitiveHasNoFinalizer) {
  static_assert(std::is_trivially_destructible<TypeWithPrimitive>::value,
                "trivially destructible");
  EXPECT_EQ(nullptr, FinalizerTrait<TypeWithPrimitive>::kCallback);
}

TEST(FinalizerTrait, FinalizerForTypeWithDestructor) {
  ExpectFinalizerIsInvoked(new TypeWithDestructor());
}

TEST(FinalizerTrait, FinalizerForTypeWithVirtualBaseDtor) {
  TypeWithVirtualDestructorBase* base = new TypeWithVirtualDestructorChild();
  ExpectFinalizerIsInvoked(base);
}

TEST(FinalizerTrait, FinalizerForCustomFinalizationMethod) {
  ExpectFinalizerIsInvoked(new TypeWithCustomFinalizationMethod());
}

TEST(FinalizerTrait, FinalizerForCustomFinalizationMethodInBase) {
  TypeWithCustomFinalizationMethodAtBase* base =
      new TypeWithCustomFinalizationMethodAtBaseChild();
  ExpectFinalizerIsInvoked(base);
}

}  // namespace internal
}  // namespace cppgc
