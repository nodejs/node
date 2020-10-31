/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Comparisions of floats is used extensively in this file. Ignore warnings
// as we want to stay close to Chromium.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/utils.h"
#include "test/gtest_and_gmock.h"

using ::testing::ElementsAre;

namespace perfetto {
namespace base {

namespace {

// Object used to test complex object with Optional<T> in addition of the move
// semantics.
class TestObject {
 public:
  enum class State {
    DEFAULT_CONSTRUCTED,
    VALUE_CONSTRUCTED,
    COPY_CONSTRUCTED,
    MOVE_CONSTRUCTED,
    MOVED_FROM,
    COPY_ASSIGNED,
    MOVE_ASSIGNED,
    SWAPPED,
  };

  TestObject() : foo_(0), bar_(0.0), state_(State::DEFAULT_CONSTRUCTED) {}

  TestObject(int foo, double bar)
      : foo_(foo), bar_(bar), state_(State::VALUE_CONSTRUCTED) {}

  TestObject(const TestObject& other)
      : foo_(other.foo_),
        bar_(other.bar_),
        state_(State::COPY_CONSTRUCTED),
        move_ctors_count_(other.move_ctors_count_) {}

  TestObject(TestObject&& other)
      : foo_(std::move(other.foo_)),
        bar_(std::move(other.bar_)),
        state_(State::MOVE_CONSTRUCTED),
        move_ctors_count_(other.move_ctors_count_ + 1) {
    other.state_ = State::MOVED_FROM;
  }

  TestObject& operator=(const TestObject& other) {
    foo_ = other.foo_;
    bar_ = other.bar_;
    state_ = State::COPY_ASSIGNED;
    move_ctors_count_ = other.move_ctors_count_;
    return *this;
  }

  TestObject& operator=(TestObject&& other) {
    foo_ = other.foo_;
    bar_ = other.bar_;
    state_ = State::MOVE_ASSIGNED;
    move_ctors_count_ = other.move_ctors_count_;
    other.state_ = State::MOVED_FROM;
    return *this;
  }

  void Swap(TestObject* other) {
    using std::swap;
    swap(foo_, other->foo_);
    swap(bar_, other->bar_);
    swap(move_ctors_count_, other->move_ctors_count_);
    state_ = State::SWAPPED;
    other->state_ = State::SWAPPED;
  }

  bool operator==(const TestObject& other) const {
    return std::tie(foo_, bar_) == std::tie(other.foo_, other.bar_);
  }

  bool operator!=(const TestObject& other) const { return !(*this == other); }

  int foo() const { return foo_; }
  State state() const { return state_; }
  int move_ctors_count() const { return move_ctors_count_; }

 private:
  int foo_;
  double bar_;
  State state_;
  int move_ctors_count_ = 0;
};

// Implementing Swappable concept.
void swap(TestObject& lhs, TestObject& rhs) {
  lhs.Swap(&rhs);
}

class NonTriviallyDestructible {
  ~NonTriviallyDestructible() {}
};

class DeletedDefaultConstructor {
 public:
  DeletedDefaultConstructor() = delete;
  DeletedDefaultConstructor(int foo) : foo_(foo) {}

  int foo() const { return foo_; }

 private:
  int foo_;
};

class DeletedCopy {
 public:
  explicit DeletedCopy(int foo) : foo_(foo) {}
  DeletedCopy(const DeletedCopy&) = delete;
  DeletedCopy(DeletedCopy&&) = default;

  DeletedCopy& operator=(const DeletedCopy&) = delete;
  DeletedCopy& operator=(DeletedCopy&&) = default;

  int foo() const { return foo_; }

 private:
  int foo_;
};

class DeletedMove {
 public:
  explicit DeletedMove(int foo) : foo_(foo) {}
  DeletedMove(const DeletedMove&) = default;
  DeletedMove(DeletedMove&&) = delete;

  DeletedMove& operator=(const DeletedMove&) = default;
  DeletedMove& operator=(DeletedMove&&) = delete;

  int foo() const { return foo_; }

 private:
  int foo_;
};

class NonTriviallyDestructibleDeletedCopyConstructor {
 public:
  explicit NonTriviallyDestructibleDeletedCopyConstructor(int foo)
      : foo_(foo) {}
  NonTriviallyDestructibleDeletedCopyConstructor(
      const NonTriviallyDestructibleDeletedCopyConstructor&) = delete;
  NonTriviallyDestructibleDeletedCopyConstructor(
      NonTriviallyDestructibleDeletedCopyConstructor&&) = default;

  ~NonTriviallyDestructibleDeletedCopyConstructor() {}

  int foo() const { return foo_; }

 private:
  int foo_;
};

class DeleteNewOperators {
 public:
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;
  void* operator new[](size_t) = delete;
  void* operator new[](size_t, void*) = delete;
};

}  // anonymous namespace

static_assert(std::is_trivially_destructible<Optional<int>>::value,
              "OptionalIsTriviallyDestructible");

static_assert(
    !std::is_trivially_destructible<Optional<NonTriviallyDestructible>>::value,
    "OptionalIsTriviallyDestructible");

static_assert(sizeof(Optional<int>) == sizeof(internal::OptionalBase<int>),
              "internal::{Copy,Move}{Constructible,Assignable} structs "
              "should be 0-sized");

TEST(OptionalTest, DefaultConstructor) {
  {
    constexpr Optional<float> o;
    EXPECT_FALSE(o);
  }

  {
    Optional<std::string> o;
    EXPECT_FALSE(o);
  }

  {
    Optional<TestObject> o;
    EXPECT_FALSE(o);
  }
}

TEST(OptionalTest, CopyConstructor) {
  {
    constexpr Optional<float> first(0.1f);
    constexpr Optional<float> other(first);

    EXPECT_TRUE(other);
    EXPECT_EQ(other.value(), 0.1f);
    EXPECT_EQ(first, other);
  }

  {
    Optional<std::string> first("foo");
    Optional<std::string> other(first);

    EXPECT_TRUE(other);
    EXPECT_EQ(other.value(), "foo");
    EXPECT_EQ(first, other);
  }

  {
    const Optional<std::string> first("foo");
    Optional<std::string> other(first);

    EXPECT_TRUE(other);
    EXPECT_EQ(other.value(), "foo");
    EXPECT_EQ(first, other);
  }

  {
    Optional<TestObject> first(TestObject(3, 0.1));
    Optional<TestObject> other(first);

    EXPECT_TRUE(!!other);
    EXPECT_TRUE(other.value() == TestObject(3, 0.1));
    EXPECT_TRUE(first == other);
  }
}

TEST(OptionalTest, ValueConstructor) {
  {
    constexpr float value = 0.1f;
    constexpr Optional<float> o(value);

    EXPECT_TRUE(o);
    EXPECT_EQ(value, o.value());
  }

  {
    std::string value("foo");
    Optional<std::string> o(value);

    EXPECT_TRUE(o);
    EXPECT_EQ(value, o.value());
  }

  {
    TestObject value(3, 0.1);
    Optional<TestObject> o(value);

    EXPECT_TRUE(o);
    EXPECT_EQ(TestObject::State::COPY_CONSTRUCTED, o->state());
    EXPECT_EQ(value, o.value());
  }
}

TEST(OptionalTest, MoveConstructor) {
  {
    constexpr Optional<float> first(0.1f);
    constexpr Optional<float> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.value(), 0.1f);

    EXPECT_TRUE(first.has_value());
  }

  {
    Optional<std::string> first("foo");
    Optional<std::string> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ("foo", second.value());

    EXPECT_TRUE(first.has_value());
  }

  {
    Optional<TestObject> first(TestObject(3, 0.1));
    Optional<TestObject> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED, second->state());
    EXPECT_TRUE(TestObject(3, 0.1) == second.value());

    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(TestObject::State::MOVED_FROM, first->state());
  }

  // Even if copy constructor is deleted, move constructor needs to work.
  // Note that it couldn't be constexpr.
  {
    Optional<DeletedCopy> first(in_place, 42);
    Optional<DeletedCopy> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42, second->foo());

    EXPECT_TRUE(first.has_value());
  }

  {
    Optional<DeletedMove> first(in_place, 42);
    Optional<DeletedMove> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42, second->foo());

    EXPECT_TRUE(first.has_value());
  }

  {
    Optional<NonTriviallyDestructibleDeletedCopyConstructor> first(in_place,
                                                                   42);
    Optional<NonTriviallyDestructibleDeletedCopyConstructor> second(
        std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42, second->foo());

    EXPECT_TRUE(first.has_value());
  }
}

TEST(OptionalTest, MoveValueConstructor) {
  {
    constexpr float value = 0.1f;
    constexpr Optional<float> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ(0.1f, o.value());
  }

  {
    float value = 0.1f;
    Optional<float> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ(0.1f, o.value());
  }

  {
    std::string value("foo");
    Optional<std::string> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ("foo", o.value());
  }

  {
    TestObject value(3, 0.1);
    Optional<TestObject> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED, o->state());
    EXPECT_EQ(TestObject(3, 0.1), o.value());
  }
}

TEST(OptionalTest, ConvertingCopyConstructor) {
  {
    Optional<int> first(1);
    Optional<double> second(first);
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(1.0, second.value());
  }

  // Make sure explicit is not marked for convertible case.
  {
    Optional<int> o(1);
    ignore_result<Optional<double>>(o);
  }
}

TEST(OptionalTest, ConvertingMoveConstructor) {
  {
    Optional<int> first(1);
    Optional<double> second(std::move(first));
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(1.0, second.value());
  }

  // Make sure explicit is not marked for convertible case.
  {
    Optional<int> o(1);
    ignore_result<Optional<double>>(std::move(o));
  }

  {
    class Test1 {
     public:
      explicit Test1(int foo) : foo_(foo) {}

      int foo() const { return foo_; }

     private:
      int foo_;
    };

    // Not copyable but convertible from Test1.
    class Test2 {
     public:
      Test2(const Test2&) = delete;
      explicit Test2(Test1&& other) : bar_(other.foo()) {}

      double bar() const { return bar_; }

     private:
      double bar_;
    };

    Optional<Test1> first(in_place, 42);
    Optional<Test2> second(std::move(first));
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42.0, second->bar());
  }
}

TEST(OptionalTest, ConstructorForwardArguments) {
  {
    constexpr Optional<float> a(base::in_place, 0.1f);
    EXPECT_TRUE(a);
    EXPECT_EQ(0.1f, a.value());
  }

  {
    Optional<float> a(base::in_place, 0.1f);
    EXPECT_TRUE(a);
    EXPECT_EQ(0.1f, a.value());
  }

  {
    Optional<std::string> a(base::in_place, "foo");
    EXPECT_TRUE(a);
    EXPECT_EQ("foo", a.value());
  }

  {
    Optional<TestObject> a(base::in_place, 0, 0.1);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(TestObject(0, 0.1) == a.value());
  }
}

TEST(OptionalTest, ConstructorForwardInitListAndArguments) {
  {
    Optional<std::vector<int>> opt(in_place, {3, 1});
    EXPECT_TRUE(opt);
    EXPECT_THAT(*opt, ElementsAre(3, 1));
    EXPECT_EQ(2u, opt->size());
  }

  {
    Optional<std::vector<int>> opt(in_place, {3, 1}, std::allocator<int>());
    EXPECT_TRUE(opt);
    EXPECT_THAT(*opt, ElementsAre(3, 1));
    EXPECT_EQ(2u, opt->size());
  }
}

TEST(OptionalTest, ForwardConstructor) {
  {
    Optional<double> a(1);
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(1.0, a.value());
  }

  // Test that default type of 'U' is value_type.
  {
    struct TestData {
      int a;
      double b;
      bool c;
    };

    Optional<TestData> a({1, 2.0, true});
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(1, a->a);
    EXPECT_EQ(2.0, a->b);
    EXPECT_TRUE(a->c);
  }

  // If T has a constructor with a param Optional<U>, and another ctor with a
  // param U, then T(Optional<U>) should be used for Optional<T>(Optional<U>)
  // constructor.
  {
    enum class ParamType {
      DEFAULT_CONSTRUCTED,
      COPY_CONSTRUCTED,
      MOVE_CONSTRUCTED,
      INT,
      IN_PLACE,
      OPTIONAL_INT,
    };
    struct Test {
      Test() : param_type(ParamType::DEFAULT_CONSTRUCTED) {}
      Test(const Test&) : param_type(ParamType::COPY_CONSTRUCTED) {}
      Test(Test&&) : param_type(ParamType::MOVE_CONSTRUCTED) {}
      explicit Test(int) : param_type(ParamType::INT) {}
      explicit Test(in_place_t) : param_type(ParamType::IN_PLACE) {}
      explicit Test(Optional<int>) : param_type(ParamType::OPTIONAL_INT) {}

      ParamType param_type;
    };

    // Overload resolution with copy-conversion constructor.
    {
      const Optional<int> arg(in_place, 1);
      Optional<Test> testee(arg);
      EXPECT_EQ(ParamType::OPTIONAL_INT, testee->param_type);
    }

    // Overload resolution with move conversion constructor.
    {
      Optional<Test> testee(Optional<int>(in_place, 1));
      EXPECT_EQ(ParamType::OPTIONAL_INT, testee->param_type);
    }

    // Default constructor should be used.
    {
      Optional<Test> testee(in_place);
      EXPECT_EQ(ParamType::DEFAULT_CONSTRUCTED, testee->param_type);
    }
  }

  {
    struct Test {
      Test(int) {}  // NOLINT(runtime/explicit)
    };
    // If T is convertible from U, it is not marked as explicit.
    static_assert(std::is_convertible<int, Test>::value,
                  "Int should be convertible to Test.");
    ([](Optional<Test>) {})(1);
  }
}

TEST(OptionalTest, NulloptConstructor) {
  constexpr Optional<int> a(base::nullopt);
  EXPECT_FALSE(a);
}

TEST(OptionalTest, AssignValue) {
  {
    Optional<float> a;
    EXPECT_FALSE(a);
    a = 0.1f;
    EXPECT_TRUE(a);

    Optional<float> b(0.1f);
    EXPECT_TRUE(a == b);
  }

  {
    Optional<std::string> a;
    EXPECT_FALSE(a);
    a = std::string("foo");
    EXPECT_TRUE(a);

    Optional<std::string> b(std::string("foo"));
    EXPECT_EQ(a, b);
  }

  {
    Optional<TestObject> a;
    EXPECT_FALSE(!!a);
    a = TestObject(3, 0.1);
    EXPECT_TRUE(!!a);

    Optional<TestObject> b(TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }

  {
    Optional<TestObject> a = TestObject(4, 1.0);
    EXPECT_TRUE(!!a);
    a = TestObject(3, 0.1);
    EXPECT_TRUE(!!a);

    Optional<TestObject> b(TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }
}

TEST(OptionalTest, AssignObject) {
  {
    Optional<float> a;
    Optional<float> b(0.1f);
    a = b;

    EXPECT_TRUE(a);
    EXPECT_EQ(a.value(), 0.1f);
    EXPECT_EQ(a, b);
  }

  {
    Optional<std::string> a;
    Optional<std::string> b("foo");
    a = b;

    EXPECT_TRUE(a);
    EXPECT_EQ(a.value(), "foo");
    EXPECT_EQ(a, b);
  }

  {
    Optional<TestObject> a;
    Optional<TestObject> b(TestObject(3, 0.1));
    a = b;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(a.value() == TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }

  {
    Optional<TestObject> a(TestObject(4, 1.0));
    Optional<TestObject> b(TestObject(3, 0.1));
    a = b;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(a.value() == TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }

  {
    Optional<DeletedMove> a(in_place, 42);
    Optional<DeletedMove> b;
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(a->foo(), b->foo());
  }

  {
    Optional<DeletedMove> a(in_place, 42);
    Optional<DeletedMove> b(in_place, 1);
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(a->foo(), b->foo());
  }

  // Converting assignment.
  {
    Optional<int> a(in_place, 1);
    Optional<double> b;
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(1, a.value());
    EXPECT_EQ(1.0, b.value());
  }

  {
    Optional<int> a(in_place, 42);
    Optional<double> b(in_place, 1);
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42, a.value());
    EXPECT_EQ(42.0, b.value());
  }

  {
    Optional<int> a;
    Optional<double> b(in_place, 1);
    b = a;
    EXPECT_FALSE(!!a);
    EXPECT_FALSE(!!b);
  }
}

TEST(OptionalTest, AssignObject_rvalue) {
  {
    Optional<float> a;
    Optional<float> b(0.1f);
    a = std::move(b);

    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    EXPECT_EQ(0.1f, a.value());
  }

  {
    Optional<std::string> a;
    Optional<std::string> b("foo");
    a = std::move(b);

    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    EXPECT_EQ("foo", a.value());
  }

  {
    Optional<TestObject> a;
    Optional<TestObject> b(TestObject(3, 0.1));
    a = std::move(b);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_TRUE(TestObject(3, 0.1) == a.value());

    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED, a->state());
    EXPECT_EQ(TestObject::State::MOVED_FROM, b->state());
  }

  {
    Optional<TestObject> a(TestObject(4, 1.0));
    Optional<TestObject> b(TestObject(3, 0.1));
    a = std::move(b);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_TRUE(TestObject(3, 0.1) == a.value());

    EXPECT_EQ(TestObject::State::MOVE_ASSIGNED, a->state());
    EXPECT_EQ(TestObject::State::MOVED_FROM, b->state());
  }

  {
    Optional<DeletedMove> a(in_place, 42);
    Optional<DeletedMove> b;
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42, b->foo());
  }

  {
    Optional<DeletedMove> a(in_place, 42);
    Optional<DeletedMove> b(in_place, 1);
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42, b->foo());
  }

  // Converting assignment.
  {
    Optional<int> a(in_place, 1);
    Optional<double> b;
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(1.0, b.value());
  }

  {
    Optional<int> a(in_place, 42);
    Optional<double> b(in_place, 1);
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42.0, b.value());
  }

  {
    Optional<int> a;
    Optional<double> b(in_place, 1);
    b = std::move(a);

    EXPECT_FALSE(!!a);
    EXPECT_FALSE(!!b);
  }
}

TEST(OptionalTest, AssignNull) {
  {
    Optional<float> a(0.1f);
    Optional<float> b(0.2f);
    a = base::nullopt;
    b = base::nullopt;
    EXPECT_EQ(a, b);
  }

  {
    Optional<std::string> a("foo");
    Optional<std::string> b("bar");
    a = base::nullopt;
    b = base::nullopt;
    EXPECT_EQ(a, b);
  }

  {
    Optional<TestObject> a(TestObject(3, 0.1));
    Optional<TestObject> b(TestObject(4, 1.0));
    a = base::nullopt;
    b = base::nullopt;
    EXPECT_TRUE(a == b);
  }
}

TEST(OptionalTest, AssignOverload) {
  struct Test1 {
    enum class State {
      CONSTRUCTED,
      MOVED,
    };
    State state = State::CONSTRUCTED;
  };

  // Here, Optional<Test2> can be assigned from Optioanl<Test1>.
  // In case of move, marks MOVED to Test1 instance.
  struct Test2 {
    enum class State {
      DEFAULT_CONSTRUCTED,
      COPY_CONSTRUCTED_FROM_TEST1,
      MOVE_CONSTRUCTED_FROM_TEST1,
      COPY_ASSIGNED_FROM_TEST1,
      MOVE_ASSIGNED_FROM_TEST1,
    };

    Test2() = default;
    explicit Test2(const Test1&) : state(State::COPY_CONSTRUCTED_FROM_TEST1) {}
    explicit Test2(Test1&& test1) : state(State::MOVE_CONSTRUCTED_FROM_TEST1) {
      test1.state = Test1::State::MOVED;
    }
    Test2& operator=(const Test1&) {
      state = State::COPY_ASSIGNED_FROM_TEST1;
      return *this;
    }
    Test2& operator=(Test1&& test1) {
      state = State::MOVE_ASSIGNED_FROM_TEST1;
      test1.state = Test1::State::MOVED;
      return *this;
    }

    State state = State::DEFAULT_CONSTRUCTED;
  };

  {
    Optional<Test1> a(in_place);
    Optional<Test2> b;

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test2::State::COPY_CONSTRUCTED_FROM_TEST1, b->state);
  }

  {
    Optional<Test1> a(in_place);
    Optional<Test2> b(in_place);

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test2::State::COPY_ASSIGNED_FROM_TEST1, b->state);
  }

  {
    Optional<Test1> a(in_place);
    Optional<Test2> b;

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test2::State::MOVE_CONSTRUCTED_FROM_TEST1, b->state);
  }

  {
    Optional<Test1> a(in_place);
    Optional<Test2> b(in_place);

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test2::State::MOVE_ASSIGNED_FROM_TEST1, b->state);
  }

  // Similar to Test2, but Test3 also has copy/move ctor and assign operators
  // from Optional<Test1>, too. In this case, for a = b where a is
  // Optional<Test3> and b is Optional<Test1>,
  // Optional<T>::operator=(U&&) where U is Optional<Test1> should be used
  // rather than Optional<T>::operator=(Optional<U>&&) where U is Test1.
  struct Test3 {
    enum class State {
      DEFAULT_CONSTRUCTED,
      COPY_CONSTRUCTED_FROM_TEST1,
      MOVE_CONSTRUCTED_FROM_TEST1,
      COPY_CONSTRUCTED_FROM_OPTIONAL_TEST1,
      MOVE_CONSTRUCTED_FROM_OPTIONAL_TEST1,
      COPY_ASSIGNED_FROM_TEST1,
      MOVE_ASSIGNED_FROM_TEST1,
      COPY_ASSIGNED_FROM_OPTIONAL_TEST1,
      MOVE_ASSIGNED_FROM_OPTIONAL_TEST1,
    };

    Test3() = default;
    explicit Test3(const Test1&) : state(State::COPY_CONSTRUCTED_FROM_TEST1) {}
    explicit Test3(Test1&& test1) : state(State::MOVE_CONSTRUCTED_FROM_TEST1) {
      test1.state = Test1::State::MOVED;
    }
    explicit Test3(const Optional<Test1>&)
        : state(State::COPY_CONSTRUCTED_FROM_OPTIONAL_TEST1) {}
    explicit Test3(Optional<Test1>&& test1)
        : state(State::MOVE_CONSTRUCTED_FROM_OPTIONAL_TEST1) {
      // In the following senarios, given |test1| should always have value.
      PERFETTO_DCHECK(test1.has_value());
      test1->state = Test1::State::MOVED;
    }
    Test3& operator=(const Test1&) {
      state = State::COPY_ASSIGNED_FROM_TEST1;
      return *this;
    }
    Test3& operator=(Test1&& test1) {
      state = State::MOVE_ASSIGNED_FROM_TEST1;
      test1.state = Test1::State::MOVED;
      return *this;
    }
    Test3& operator=(const Optional<Test1>&) {
      state = State::COPY_ASSIGNED_FROM_OPTIONAL_TEST1;
      return *this;
    }
    Test3& operator=(Optional<Test1>&& test1) {
      state = State::MOVE_ASSIGNED_FROM_OPTIONAL_TEST1;
      // In the following senarios, given |test1| should always have value.
      PERFETTO_DCHECK(test1.has_value());
      test1->state = Test1::State::MOVED;
      return *this;
    }

    State state = State::DEFAULT_CONSTRUCTED;
  };

  {
    Optional<Test1> a(in_place);
    Optional<Test3> b;

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test3::State::COPY_CONSTRUCTED_FROM_OPTIONAL_TEST1, b->state);
  }

  {
    Optional<Test1> a(in_place);
    Optional<Test3> b(in_place);

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test3::State::COPY_ASSIGNED_FROM_OPTIONAL_TEST1, b->state);
  }

  {
    Optional<Test1> a(in_place);
    Optional<Test3> b;

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test3::State::MOVE_CONSTRUCTED_FROM_OPTIONAL_TEST1, b->state);
  }

  {
    Optional<Test1> a(in_place);
    Optional<Test3> b(in_place);

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test3::State::MOVE_ASSIGNED_FROM_OPTIONAL_TEST1, b->state);
  }
}

TEST(OptionalTest, OperatorStar) {
  {
    Optional<float> a(0.1f);
    EXPECT_EQ(a.value(), *a);
  }

  {
    Optional<std::string> a("foo");
    EXPECT_EQ(a.value(), *a);
  }

  {
    Optional<TestObject> a(TestObject(3, 0.1));
    EXPECT_EQ(a.value(), *a);
  }
}

TEST(OptionalTest, OperatorStar_rvalue) {
  EXPECT_EQ(0.1f, *Optional<float>(0.1f));
  EXPECT_EQ(std::string("foo"), *Optional<std::string>("foo"));
  EXPECT_TRUE(TestObject(3, 0.1) == *Optional<TestObject>(TestObject(3, 0.1)));
}

TEST(OptionalTest, OperatorArrow) {
  Optional<TestObject> a(TestObject(3, 0.1));
  EXPECT_EQ(a->foo(), 3);
}

TEST(OptionalTest, Value_rvalue) {
  EXPECT_EQ(0.1f, Optional<float>(0.1f).value());
  EXPECT_EQ(std::string("foo"), Optional<std::string>("foo").value());
  EXPECT_TRUE(TestObject(3, 0.1) ==
              Optional<TestObject>(TestObject(3, 0.1)).value());
}

TEST(OptionalTest, ValueOr) {
  {
    Optional<float> a;
    EXPECT_EQ(0.0f, a.value_or(0.0f));

    a = 0.1f;
    EXPECT_EQ(0.1f, a.value_or(0.0f));

    a = base::nullopt;
    EXPECT_EQ(0.0f, a.value_or(0.0f));
  }

  // value_or() can be constexpr.
  {
    constexpr Optional<int> a(in_place, 1);
    constexpr int value = a.value_or(10);
    EXPECT_EQ(1, value);
  }
  {
    constexpr Optional<int> a;
    constexpr int value = a.value_or(10);
    EXPECT_EQ(10, value);
  }

  {
    Optional<std::string> a;
    EXPECT_EQ("bar", a.value_or("bar"));

    a = std::string("foo");
    EXPECT_EQ(std::string("foo"), a.value_or("bar"));

    a = base::nullopt;
    EXPECT_EQ(std::string("bar"), a.value_or("bar"));
  }

  {
    Optional<TestObject> a;
    EXPECT_TRUE(a.value_or(TestObject(1, 0.3)) == TestObject(1, 0.3));

    a = TestObject(3, 0.1);
    EXPECT_TRUE(a.value_or(TestObject(1, 0.3)) == TestObject(3, 0.1));

    a = base::nullopt;
    EXPECT_TRUE(a.value_or(TestObject(1, 0.3)) == TestObject(1, 0.3));
  }
}

TEST(OptionalTest, Swap_bothNoValue) {
  Optional<TestObject> a, b;
  a.swap(b);

  EXPECT_FALSE(a);
  EXPECT_FALSE(b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, Swap_inHasValue) {
  Optional<TestObject> a(TestObject(1, 0.3));
  Optional<TestObject> b;
  a.swap(b);

  EXPECT_FALSE(a);

  EXPECT_TRUE(!!b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(1, 0.3) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, Swap_outHasValue) {
  Optional<TestObject> a;
  Optional<TestObject> b(TestObject(1, 0.3));
  a.swap(b);

  EXPECT_TRUE(!!a);
  EXPECT_FALSE(!!b);
  EXPECT_TRUE(TestObject(1, 0.3) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, Swap_bothValue) {
  Optional<TestObject> a(TestObject(0, 0.1));
  Optional<TestObject> b(TestObject(1, 0.3));
  a.swap(b);

  EXPECT_TRUE(!!a);
  EXPECT_TRUE(!!b);
  EXPECT_TRUE(TestObject(1, 0.3) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(0, 0.1) == b.value_or(TestObject(42, 0.42)));
  EXPECT_EQ(TestObject::State::SWAPPED, a->state());
  EXPECT_EQ(TestObject::State::SWAPPED, b->state());
}

TEST(OptionalTest, Emplace) {
  {
    Optional<float> a(0.1f);
    EXPECT_EQ(0.3f, a.emplace(0.3f));

    EXPECT_TRUE(a);
    EXPECT_EQ(0.3f, a.value());
  }

  {
    Optional<std::string> a("foo");
    EXPECT_EQ("bar", a.emplace("bar"));

    EXPECT_TRUE(a);
    EXPECT_EQ("bar", a.value());
  }

  {
    Optional<TestObject> a(TestObject(0, 0.1));
    EXPECT_EQ(TestObject(1, 0.2), a.emplace(TestObject(1, 0.2)));

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(TestObject(1, 0.2) == a.value());
  }

  {
    Optional<std::vector<int>> a;
    auto& ref = a.emplace({2, 3});
    static_assert(std::is_same<std::vector<int>&, decltype(ref)>::value, "");
    EXPECT_TRUE(a);
    EXPECT_THAT(*a, ElementsAre(2, 3));
    EXPECT_EQ(&ref, &*a);
  }

  {
    Optional<std::vector<int>> a;
    auto& ref = a.emplace({4, 5}, std::allocator<int>());
    static_assert(std::is_same<std::vector<int>&, decltype(ref)>::value, "");
    EXPECT_TRUE(a);
    EXPECT_THAT(*a, ElementsAre(4, 5));
    EXPECT_EQ(&ref, &*a);
  }
}

TEST(OptionalTest, Equals_TwoEmpty) {
  Optional<int> a;
  Optional<int> b;

  EXPECT_TRUE(a == b);
}

TEST(OptionalTest, Equals_TwoEquals) {
  Optional<int> a(1);
  Optional<int> b(1);

  EXPECT_TRUE(a == b);
}

TEST(OptionalTest, Equals_OneEmpty) {
  Optional<int> a;
  Optional<int> b(1);

  EXPECT_FALSE(a == b);
}

TEST(OptionalTest, Equals_TwoDifferent) {
  Optional<int> a(0);
  Optional<int> b(1);

  EXPECT_FALSE(a == b);
}

TEST(OptionalTest, Equals_DifferentType) {
  Optional<int> a(0);
  Optional<double> b(0);

  EXPECT_TRUE(a == b);
}

TEST(OptionalTest, NotEquals_TwoEmpty) {
  Optional<int> a;
  Optional<int> b;

  EXPECT_FALSE(a != b);
}

TEST(OptionalTest, NotEquals_TwoEquals) {
  Optional<int> a(1);
  Optional<int> b(1);

  EXPECT_FALSE(a != b);
}

TEST(OptionalTest, NotEquals_OneEmpty) {
  Optional<int> a;
  Optional<int> b(1);

  EXPECT_TRUE(a != b);
}

TEST(OptionalTest, NotEquals_TwoDifferent) {
  Optional<int> a(0);
  Optional<int> b(1);

  EXPECT_TRUE(a != b);
}

TEST(OptionalTest, NotEquals_DifferentType) {
  Optional<int> a(0);
  Optional<double> b(0.0);

  EXPECT_FALSE(a != b);
}

TEST(OptionalTest, Less_LeftEmpty) {
  Optional<int> l;
  Optional<int> r(1);

  EXPECT_TRUE(l < r);
}

TEST(OptionalTest, Less_RightEmpty) {
  Optional<int> l(1);
  Optional<int> r;

  EXPECT_FALSE(l < r);
}

TEST(OptionalTest, Less_BothEmpty) {
  Optional<int> l;
  Optional<int> r;

  EXPECT_FALSE(l < r);
}

TEST(OptionalTest, Less_BothValues) {
  {
    Optional<int> l(1);
    Optional<int> r(2);

    EXPECT_TRUE(l < r);
  }
  {
    Optional<int> l(2);
    Optional<int> r(1);

    EXPECT_FALSE(l < r);
  }
  {
    Optional<int> l(1);
    Optional<int> r(1);

    EXPECT_FALSE(l < r);
  }
}

TEST(OptionalTest, Less_DifferentType) {
  Optional<int> l(1);
  Optional<double> r(2.0);

  EXPECT_TRUE(l < r);
}

TEST(OptionalTest, LessEq_LeftEmpty) {
  Optional<int> l;
  Optional<int> r(1);

  EXPECT_TRUE(l <= r);
}

TEST(OptionalTest, LessEq_RightEmpty) {
  Optional<int> l(1);
  Optional<int> r;

  EXPECT_FALSE(l <= r);
}

TEST(OptionalTest, LessEq_BothEmpty) {
  Optional<int> l;
  Optional<int> r;

  EXPECT_TRUE(l <= r);
}

TEST(OptionalTest, LessEq_BothValues) {
  {
    Optional<int> l(1);
    Optional<int> r(2);

    EXPECT_TRUE(l <= r);
  }
  {
    Optional<int> l(2);
    Optional<int> r(1);

    EXPECT_FALSE(l <= r);
  }
  {
    Optional<int> l(1);
    Optional<int> r(1);

    EXPECT_TRUE(l <= r);
  }
}

TEST(OptionalTest, LessEq_DifferentType) {
  Optional<int> l(1);
  Optional<double> r(2.0);

  EXPECT_TRUE(l <= r);
}

TEST(OptionalTest, Greater_BothEmpty) {
  Optional<int> l;
  Optional<int> r;

  EXPECT_FALSE(l > r);
}

TEST(OptionalTest, Greater_LeftEmpty) {
  Optional<int> l;
  Optional<int> r(1);

  EXPECT_FALSE(l > r);
}

TEST(OptionalTest, Greater_RightEmpty) {
  Optional<int> l(1);
  Optional<int> r;

  EXPECT_TRUE(l > r);
}

TEST(OptionalTest, Greater_BothValue) {
  {
    Optional<int> l(1);
    Optional<int> r(2);

    EXPECT_FALSE(l > r);
  }
  {
    Optional<int> l(2);
    Optional<int> r(1);

    EXPECT_TRUE(l > r);
  }
  {
    Optional<int> l(1);
    Optional<int> r(1);

    EXPECT_FALSE(l > r);
  }
}

TEST(OptionalTest, Greater_DifferentType) {
  Optional<int> l(1);
  Optional<double> r(2.0);

  EXPECT_FALSE(l > r);
}

TEST(OptionalTest, GreaterEq_BothEmpty) {
  Optional<int> l;
  Optional<int> r;

  EXPECT_TRUE(l >= r);
}

TEST(OptionalTest, GreaterEq_LeftEmpty) {
  Optional<int> l;
  Optional<int> r(1);

  EXPECT_FALSE(l >= r);
}

TEST(OptionalTest, GreaterEq_RightEmpty) {
  Optional<int> l(1);
  Optional<int> r;

  EXPECT_TRUE(l >= r);
}

TEST(OptionalTest, GreaterEq_BothValue) {
  {
    Optional<int> l(1);
    Optional<int> r(2);

    EXPECT_FALSE(l >= r);
  }
  {
    Optional<int> l(2);
    Optional<int> r(1);

    EXPECT_TRUE(l >= r);
  }
  {
    Optional<int> l(1);
    Optional<int> r(1);

    EXPECT_TRUE(l >= r);
  }
}

TEST(OptionalTest, GreaterEq_DifferentType) {
  Optional<int> l(1);
  Optional<double> r(2.0);

  EXPECT_FALSE(l >= r);
}

TEST(OptionalTest, OptNullEq) {
  {
    Optional<int> opt;
    EXPECT_TRUE(opt == base::nullopt);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(opt == base::nullopt);
  }
}

TEST(OptionalTest, NullOptEq) {
  {
    Optional<int> opt;
    EXPECT_TRUE(base::nullopt == opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(base::nullopt == opt);
  }
}

TEST(OptionalTest, OptNullNotEq) {
  {
    Optional<int> opt;
    EXPECT_FALSE(opt != base::nullopt);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(opt != base::nullopt);
  }
}

TEST(OptionalTest, NullOptNotEq) {
  {
    Optional<int> opt;
    EXPECT_FALSE(base::nullopt != opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(base::nullopt != opt);
  }
}

TEST(OptionalTest, OptNullLower) {
  {
    Optional<int> opt;
    EXPECT_FALSE(opt < base::nullopt);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(opt < base::nullopt);
  }
}

TEST(OptionalTest, NullOptLower) {
  {
    Optional<int> opt;
    EXPECT_FALSE(base::nullopt < opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(base::nullopt < opt);
  }
}

TEST(OptionalTest, OptNullLowerEq) {
  {
    Optional<int> opt;
    EXPECT_TRUE(opt <= base::nullopt);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(opt <= base::nullopt);
  }
}

TEST(OptionalTest, NullOptLowerEq) {
  {
    Optional<int> opt;
    EXPECT_TRUE(base::nullopt <= opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(base::nullopt <= opt);
  }
}

TEST(OptionalTest, OptNullGreater) {
  {
    Optional<int> opt;
    EXPECT_FALSE(opt > base::nullopt);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(opt > base::nullopt);
  }
}

TEST(OptionalTest, NullOptGreater) {
  {
    Optional<int> opt;
    EXPECT_FALSE(base::nullopt > opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(base::nullopt > opt);
  }
}

TEST(OptionalTest, OptNullGreaterEq) {
  {
    Optional<int> opt;
    EXPECT_TRUE(opt >= base::nullopt);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(opt >= base::nullopt);
  }
}

TEST(OptionalTest, NullOptGreaterEq) {
  {
    Optional<int> opt;
    EXPECT_TRUE(base::nullopt >= opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(base::nullopt >= opt);
  }
}

TEST(OptionalTest, ValueEq_Empty) {
  Optional<int> opt;
  EXPECT_FALSE(opt == 1);
}

TEST(OptionalTest, ValueEq_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_FALSE(opt == 1);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(opt == 1);
  }
}

TEST(OptionalTest, ValueEq_DifferentType) {
  Optional<int> opt(0);
  EXPECT_TRUE(opt == 0.0);
}

TEST(OptionalTest, EqValue_Empty) {
  Optional<int> opt;
  EXPECT_FALSE(1 == opt);
}

TEST(OptionalTest, EqValue_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_FALSE(1 == opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(1 == opt);
  }
}

TEST(OptionalTest, EqValue_DifferentType) {
  Optional<int> opt(0);
  EXPECT_TRUE(0.0 == opt);
}

TEST(OptionalTest, ValueNotEq_Empty) {
  Optional<int> opt;
  EXPECT_TRUE(opt != 1);
}

TEST(OptionalTest, ValueNotEq_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_TRUE(opt != 1);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(opt != 1);
  }
}

TEST(OPtionalTest, ValueNotEq_DifferentType) {
  Optional<int> opt(0);
  EXPECT_FALSE(opt != 0.0);
}

TEST(OptionalTest, NotEqValue_Empty) {
  Optional<int> opt;
  EXPECT_TRUE(1 != opt);
}

TEST(OptionalTest, NotEqValue_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_TRUE(1 != opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(1 != opt);
  }
}

TEST(OptionalTest, NotEqValue_DifferentType) {
  Optional<int> opt(0);
  EXPECT_FALSE(0.0 != opt);
}

TEST(OptionalTest, ValueLess_Empty) {
  Optional<int> opt;
  EXPECT_TRUE(opt < 1);
}

TEST(OptionalTest, ValueLess_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_TRUE(opt < 1);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(opt < 1);
  }
  {
    Optional<int> opt(2);
    EXPECT_FALSE(opt < 1);
  }
}

TEST(OPtionalTest, ValueLess_DifferentType) {
  Optional<int> opt(0);
  EXPECT_TRUE(opt < 1.0);
}

TEST(OptionalTest, LessValue_Empty) {
  Optional<int> opt;
  EXPECT_FALSE(1 < opt);
}

TEST(OptionalTest, LessValue_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_FALSE(1 < opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(1 < opt);
  }
  {
    Optional<int> opt(2);
    EXPECT_TRUE(1 < opt);
  }
}

TEST(OptionalTest, LessValue_DifferentType) {
  Optional<int> opt(0);
  EXPECT_FALSE(0.0 < opt);
}

TEST(OptionalTest, ValueLessEq_Empty) {
  Optional<int> opt;
  EXPECT_TRUE(opt <= 1);
}

TEST(OptionalTest, ValueLessEq_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_TRUE(opt <= 1);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(opt <= 1);
  }
  {
    Optional<int> opt(2);
    EXPECT_FALSE(opt <= 1);
  }
}

TEST(OptionalTest, ValueLessEq_DifferentType) {
  Optional<int> opt(0);
  EXPECT_TRUE(opt <= 0.0);
}

TEST(OptionalTest, LessEqValue_Empty) {
  Optional<int> opt;
  EXPECT_FALSE(1 <= opt);
}

TEST(OptionalTest, LessEqValue_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_FALSE(1 <= opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(1 <= opt);
  }
  {
    Optional<int> opt(2);
    EXPECT_TRUE(1 <= opt);
  }
}

TEST(OptionalTest, LessEqValue_DifferentType) {
  Optional<int> opt(0);
  EXPECT_TRUE(0.0 <= opt);
}

TEST(OptionalTest, ValueGreater_Empty) {
  Optional<int> opt;
  EXPECT_FALSE(opt > 1);
}

TEST(OptionalTest, ValueGreater_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_FALSE(opt > 1);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(opt > 1);
  }
  {
    Optional<int> opt(2);
    EXPECT_TRUE(opt > 1);
  }
}

TEST(OptionalTest, ValueGreater_DifferentType) {
  Optional<int> opt(0);
  EXPECT_FALSE(opt > 0.0);
}

TEST(OptionalTest, GreaterValue_Empty) {
  Optional<int> opt;
  EXPECT_TRUE(1 > opt);
}

TEST(OptionalTest, GreaterValue_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_TRUE(1 > opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_FALSE(1 > opt);
  }
  {
    Optional<int> opt(2);
    EXPECT_FALSE(1 > opt);
  }
}

TEST(OptionalTest, GreaterValue_DifferentType) {
  Optional<int> opt(0);
  EXPECT_FALSE(0.0 > opt);
}

TEST(OptionalTest, ValueGreaterEq_Empty) {
  Optional<int> opt;
  EXPECT_FALSE(opt >= 1);
}

TEST(OptionalTest, ValueGreaterEq_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_FALSE(opt >= 1);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(opt >= 1);
  }
  {
    Optional<int> opt(2);
    EXPECT_TRUE(opt >= 1);
  }
}

TEST(OptionalTest, ValueGreaterEq_DifferentType) {
  Optional<int> opt(0);
  EXPECT_TRUE(opt <= 0.0);
}

TEST(OptionalTest, GreaterEqValue_Empty) {
  Optional<int> opt;
  EXPECT_TRUE(1 >= opt);
}

TEST(OptionalTest, GreaterEqValue_NotEmpty) {
  {
    Optional<int> opt(0);
    EXPECT_TRUE(1 >= opt);
  }
  {
    Optional<int> opt(1);
    EXPECT_TRUE(1 >= opt);
  }
  {
    Optional<int> opt(2);
    EXPECT_FALSE(1 >= opt);
  }
}

TEST(OptionalTest, GreaterEqValue_DifferentType) {
  Optional<int> opt(0);
  EXPECT_TRUE(0.0 >= opt);
}

TEST(OptionalTest, NotEquals) {
  {
    Optional<float> a(0.1f);
    Optional<float> b(0.2f);
    EXPECT_NE(a, b);
  }

  {
    Optional<std::string> a("foo");
    Optional<std::string> b("bar");
    EXPECT_NE(a, b);
  }

  {
    Optional<int> a(1);
    Optional<double> b(2);
    EXPECT_NE(a, b);
  }

  {
    Optional<TestObject> a(TestObject(3, 0.1));
    Optional<TestObject> b(TestObject(4, 1.0));
    EXPECT_TRUE(a != b);
  }
}

TEST(OptionalTest, NotEqualsNull) {
  {
    Optional<float> a(0.1f);
    Optional<float> b(0.1f);
    b = base::nullopt;
    EXPECT_NE(a, b);
  }

  {
    Optional<std::string> a("foo");
    Optional<std::string> b("foo");
    b = base::nullopt;
    EXPECT_NE(a, b);
  }

  {
    Optional<TestObject> a(TestObject(3, 0.1));
    Optional<TestObject> b(TestObject(3, 0.1));
    b = base::nullopt;
    EXPECT_TRUE(a != b);
  }
}

TEST(OptionalTest, MakeOptional) {
  {
    // Use qualified base::make_optional here and elsewhere to avoid the name
    // confliction to std::make_optional.
    // The name conflict happens only for types in std namespace, such as
    // std::string. The other qualified base::make_optional usages are just for
    // consistency.
    Optional<float> o = base::make_optional(32.f);
    EXPECT_TRUE(o);
    EXPECT_EQ(32.f, *o);

    float value = 3.f;
    o = base::make_optional(std::move(value));
    EXPECT_TRUE(o);
    EXPECT_EQ(3.f, *o);
  }

  {
    Optional<std::string> o = base::make_optional(std::string("foo"));
    EXPECT_TRUE(o);
    EXPECT_EQ("foo", *o);

    std::string value = "bar";
    o = base::make_optional(std::move(value));
    EXPECT_TRUE(o);
    EXPECT_EQ(std::string("bar"), *o);
  }

  {
    Optional<TestObject> o = base::make_optional(TestObject(3, 0.1));
    EXPECT_TRUE(!!o);
    EXPECT_TRUE(TestObject(3, 0.1) == *o);

    TestObject value = TestObject(0, 0.42);
    o = base::make_optional(std::move(value));
    EXPECT_TRUE(!!o);
    EXPECT_TRUE(TestObject(0, 0.42) == *o);
    EXPECT_EQ(TestObject::State::MOVED_FROM, value.state());
    EXPECT_EQ(TestObject::State::MOVE_ASSIGNED, o->state());

    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED,
              base::make_optional(std::move(value))->state());
  }

  {
    struct Test {
      Test(int x, double y, bool z) : a(x), b(y), c(z) {}

      int a;
      double b;
      bool c;
    };

    Optional<Test> o = base::make_optional<Test>(1, 2.0, true);
    EXPECT_TRUE(!!o);
    EXPECT_EQ(1, o->a);
    EXPECT_EQ(2.0, o->b);
    EXPECT_TRUE(o->c);
  }

  {
    auto str1 = base::make_optional<std::string>({'1', '2', '3'});
    EXPECT_EQ("123", *str1);

    auto str2 = base::make_optional<std::string>({'a', 'b', 'c'},
                                                 std::allocator<char>());
    EXPECT_EQ("abc", *str2);
  }
}

TEST(OptionalTest, NonMemberSwap_bothNoValue) {
  Optional<TestObject> a, b;
  base::swap(a, b);

  EXPECT_FALSE(!!a);
  EXPECT_FALSE(!!b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, NonMemberSwap_inHasValue) {
  Optional<TestObject> a(TestObject(1, 0.3));
  Optional<TestObject> b;
  base::swap(a, b);

  EXPECT_FALSE(!!a);
  EXPECT_TRUE(!!b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(1, 0.3) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, NonMemberSwap_outHasValue) {
  Optional<TestObject> a;
  Optional<TestObject> b(TestObject(1, 0.3));
  base::swap(a, b);

  EXPECT_TRUE(!!a);
  EXPECT_FALSE(!!b);
  EXPECT_TRUE(TestObject(1, 0.3) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, NonMemberSwap_bothValue) {
  Optional<TestObject> a(TestObject(0, 0.1));
  Optional<TestObject> b(TestObject(1, 0.3));
  base::swap(a, b);

  EXPECT_TRUE(!!a);
  EXPECT_TRUE(!!b);
  EXPECT_TRUE(TestObject(1, 0.3) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(0, 0.1) == b.value_or(TestObject(42, 0.42)));
  EXPECT_EQ(TestObject::State::SWAPPED, a->state());
  EXPECT_EQ(TestObject::State::SWAPPED, b->state());
}

TEST(OptionalTest, Hash_OptionalReflectsInternal) {
  {
    std::hash<int> int_hash;
    std::hash<Optional<int>> opt_int_hash;

    EXPECT_EQ(int_hash(1), opt_int_hash(Optional<int>(1)));
  }

  {
    std::hash<std::string> str_hash;
    std::hash<Optional<std::string>> opt_str_hash;

    EXPECT_EQ(str_hash(std::string("foobar")),
              opt_str_hash(Optional<std::string>(std::string("foobar"))));
  }
}

TEST(OptionalTest, Hash_NullOptEqualsNullOpt) {
  std::hash<Optional<int>> opt_int_hash;
  std::hash<Optional<std::string>> opt_str_hash;

  EXPECT_EQ(opt_str_hash(Optional<std::string>()),
            opt_int_hash(Optional<int>()));
}

TEST(OptionalTest, Hash_UseInSet) {
  std::set<Optional<int>> setOptInt;

  EXPECT_EQ(setOptInt.end(), setOptInt.find(42));

  setOptInt.insert(Optional<int>(3));
  EXPECT_EQ(setOptInt.end(), setOptInt.find(42));
  EXPECT_NE(setOptInt.end(), setOptInt.find(3));
}

TEST(OptionalTest, HasValue) {
  Optional<int> a;
  EXPECT_FALSE(a.has_value());

  a = 42;
  EXPECT_TRUE(a.has_value());

  a = nullopt;
  EXPECT_FALSE(a.has_value());

  a = 0;
  EXPECT_TRUE(a.has_value());

  a = Optional<int>();
  EXPECT_FALSE(a.has_value());
}

TEST(OptionalTest, Reset_int) {
  Optional<int> a(0);
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(0, a.value());

  a.reset();
  EXPECT_FALSE(a.has_value());
  EXPECT_EQ(-1, a.value_or(-1));
}

TEST(OptionalTest, Reset_Object) {
  Optional<TestObject> a(TestObject(0, 0.1));
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(TestObject(0, 0.1), a.value());

  a.reset();
  EXPECT_FALSE(a.has_value());
  EXPECT_EQ(TestObject(42, 0.0), a.value_or(TestObject(42, 0.0)));
}

TEST(OptionalTest, Reset_NoOp) {
  Optional<int> a;
  EXPECT_FALSE(a.has_value());

  a.reset();
  EXPECT_FALSE(a.has_value());
}

TEST(OptionalTest, AssignFromRValue) {
  Optional<TestObject> a;
  EXPECT_FALSE(a.has_value());

  TestObject obj;
  a = std::move(obj);
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(1, a->move_ctors_count());
}

TEST(OptionalTest, DontCallDefaultCtor) {
  Optional<DeletedDefaultConstructor> a;
  EXPECT_FALSE(a.has_value());

  a = base::make_optional<DeletedDefaultConstructor>(42);
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(42, a->foo());
}

TEST(OptionalTest, DontCallNewMemberFunction) {
  Optional<DeleteNewOperators> a;
  EXPECT_FALSE(a.has_value());

  a = DeleteNewOperators();
  EXPECT_TRUE(a.has_value());
}

TEST(OptionalTest, Noexcept) {
  // Trivial copy ctor, non-trivial move ctor, nothrow move assign.
  struct Test1 {
    Test1(const Test1&) = default;
    Test1(Test1&&) {}
    Test1& operator=(Test1&&) = default;
  };
  // Non-trivial copy ctor, trivial move ctor, throw move assign.
  struct Test2 {
    Test2(const Test2&) {}
    Test2(Test2&&) = default;
    Test2& operator=(Test2&&) { return *this; }
  };
  // Trivial copy ctor, non-trivial nothrow move ctor.
  struct Test3 {
    Test3(const Test3&) = default;
    Test3(Test3&&) noexcept {}
  };
  // Non-trivial copy ctor, non-trivial nothrow move ctor.
  struct Test4 {
    Test4(const Test4&) {}
    Test4(Test4&&) noexcept {}
  };
  // Non-trivial copy ctor, non-trivial move ctor.
  struct Test5 {
    Test5(const Test5&) {}
    Test5(Test5&&) {}
  };

  static_assert(
      noexcept(Optional<int>(std::declval<Optional<int>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(trivial copy, trivial move)");
  static_assert(
      !noexcept(Optional<Test1>(std::declval<Optional<Test1>>())),
      "move constructor for non-noexcept move-constructible T must not be "
      "noexcept (trivial copy)");
  static_assert(
      noexcept(Optional<Test2>(std::declval<Optional<Test2>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(non-trivial copy, trivial move)");
  static_assert(
      noexcept(Optional<Test3>(std::declval<Optional<Test3>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(trivial copy, non-trivial move)");
  static_assert(
      noexcept(Optional<Test4>(std::declval<Optional<Test4>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(non-trivial copy, non-trivial move)");
  static_assert(
      !noexcept(Optional<Test5>(std::declval<Optional<Test5>>())),
      "move constructor for non-noexcept move-constructible T must not be "
      "noexcept (non-trivial copy)");

  static_assert(
      noexcept(std::declval<Optional<int>>() = std::declval<Optional<int>>()),
      "move assign for noexcept move-constructible/move-assignable T "
      "must be noexcept");
  static_assert(
      !noexcept(std::declval<Optional<Test1>>() =
                    std::declval<Optional<Test1>>()),
      "move assign for non-noexcept move-constructible T must not be noexcept");
  static_assert(
      !noexcept(std::declval<Optional<Test2>>() =
                    std::declval<Optional<Test2>>()),
      "move assign for non-noexcept move-assignable T must not be noexcept");
}

}  // namespace base
}  // namespace perfetto

#pragma GCC diagnostic pop
