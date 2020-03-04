// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/torque-compiler.h"
#include "src/torque/utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace torque {

namespace {

// This is a simplified version of the basic Torque type definitions.
// Some class types are replaced by abstact types to keep it self-contained and
// small.
constexpr const char* kTestTorquePrelude = R"(
type void;
type never;

namespace torque_internal {
  struct Reference<T: type> {
    const object: HeapObject;
    const offset: intptr;
  }
}

type Tagged generates 'TNode<MaybeObject>' constexpr 'MaybeObject';
type StrongTagged extends Tagged
    generates 'TNode<Object>' constexpr 'ObjectPtr';
type Smi extends StrongTagged generates 'TNode<Smi>' constexpr 'Smi';
type WeakHeapObject extends Tagged;
type Weak<T : type extends HeapObject> extends WeakHeapObject;
type Uninitialized extends Tagged;

@abstract
extern class HeapObject extends StrongTagged {
  map: Map;
}
type Map extends HeapObject generates 'TNode<Map>';
type Object = Smi | HeapObject;
type JSReceiver extends HeapObject generates 'TNode<JSReceiver>';
type JSObject extends JSReceiver generates 'TNode<JSObject>';
type int32 generates 'TNode<Int32T>' constexpr 'int32_t';
type uint32 generates 'TNode<Uint32T>' constexpr 'uint32_t';
type int31 extends int32
    generates 'TNode<Int32T>' constexpr 'int31_t';
type uint31 extends uint32
    generates 'TNode<Uint32T>' constexpr 'uint31_t';
type int16 extends int31
    generates 'TNode<Int16T>' constexpr 'int16_t';
type uint16 extends uint31
    generates 'TNode<Uint16T>' constexpr 'uint16_t';
type int8 extends int16 generates 'TNode<Int8T>' constexpr 'int8_t';
type uint8 extends uint16
    generates 'TNode<Uint8T>' constexpr 'uint8_t';
type int64 generates 'TNode<Int64T>' constexpr 'int64_t';
type intptr generates 'TNode<IntPtrT>' constexpr 'intptr_t';
type uintptr generates 'TNode<UintPtrT>' constexpr 'uintptr_t';
type float32 generates 'TNode<Float32T>' constexpr 'float';
type float64 generates 'TNode<Float64T>' constexpr 'double';
type bool generates 'TNode<BoolT>' constexpr 'bool';
type bint generates 'TNode<BInt>' constexpr 'BInt';
type string constexpr 'const char*';
type RawPtr generates 'TNode<RawPtrT>' constexpr 'void*';
type Code extends HeapObject generates 'TNode<Code>';
type BuiltinPtr extends Smi generates 'TNode<BuiltinPtr>';
type Context extends HeapObject generates 'TNode<Context>';
type NativeContext extends Context;

macro FromConstexpr<To: type, From: type>(o: From): To;
)";

TorqueCompilerResult TestCompileTorque(std::string source) {
  TorqueCompilerOptions options;
  options.output_directory = "";
  options.collect_language_server_data = false;
  options.force_assert_statements = false;
  options.v8_root = ".";

  source = kTestTorquePrelude + source;
  return CompileTorque(source, options);
}

void ExpectSuccessfulCompilation(std::string source) {
  TorqueCompilerResult result = TestCompileTorque(std::move(source));
  std::vector<std::string> messages;
  for (const auto& message : result.messages) {
    messages.push_back(message.message);
  }
  EXPECT_EQ(messages, std::vector<std::string>{});
}

template <class T>
using MatcherVector =
    std::vector<std::pair<::testing::PolymorphicMatcher<T>, LineAndColumn>>;

template <class T>
void ExpectFailingCompilation(std::string source,
                              MatcherVector<T> message_patterns) {
  TorqueCompilerResult result = TestCompileTorque(std::move(source));
  ASSERT_FALSE(result.messages.empty());
  EXPECT_GE(result.messages.size(), message_patterns.size());
  size_t limit = message_patterns.size();
  if (result.messages.size() < limit) {
    limit = result.messages.size();
  }
  for (size_t i = 0; i < limit; ++i) {
    EXPECT_THAT(result.messages[i].message, message_patterns[i].first);
    if (message_patterns[i].second != LineAndColumn::Invalid()) {
      base::Optional<SourcePosition> actual = result.messages[i].position;
      EXPECT_TRUE(actual.has_value());
      EXPECT_EQ(actual->start, message_patterns[i].second);
    }
  }
}

template <class T>
void ExpectFailingCompilation(
    std::string source, ::testing::PolymorphicMatcher<T> message_pattern) {
  ExpectFailingCompilation(
      source, MatcherVector<T>{{message_pattern, LineAndColumn::Invalid()}});
}

int CountPreludeLines() {
  static int result = -1;
  if (result == -1) {
    std::string prelude(kTestTorquePrelude);
    result = static_cast<int>(std::count(prelude.begin(), prelude.end(), '\n'));
  }
  return result;
}

using SubstrWithPosition =
    std::pair<::testing::PolymorphicMatcher<
                  ::testing::internal::HasSubstrMatcher<std::string>>,
              LineAndColumn>;

SubstrWithPosition SubstrTester(const std::string& message, int line, int col) {
  // Change line and column from 1-based to 0-based.
  return {::testing::HasSubstr(message),
          LineAndColumn{line + CountPreludeLines() - 1, col - 1}};
}

using SubstrVector = std::vector<SubstrWithPosition>;

}  // namespace

TEST(Torque, Prelude) { ExpectSuccessfulCompilation(""); }

TEST(Torque, StackDeleteRange) {
  Stack<int> stack = {1, 2, 3, 4, 5, 6, 7};
  stack.DeleteRange(StackRange{BottomOffset{2}, BottomOffset{4}});
  Stack<int> result = {1, 2, 5, 6, 7};
  ASSERT_TRUE(stack == result);
}

using ::testing::HasSubstr;
TEST(Torque, TypeNamingConventionLintError) {
  ExpectFailingCompilation(R"(
    type foo generates 'TNode<Foo>';
  )",
                           HasSubstr("\"foo\""));
}

TEST(Torque, StructNamingConventionLintError) {
  ExpectFailingCompilation(R"(
    struct foo {}
  )",
                           HasSubstr("\"foo\""));
}

TEST(Torque, ClassDefinition) {
  ExpectSuccessfulCompilation(R"(
    extern class TestClassWithAllTypes extends HeapObject {
      a: int8;
      b: uint8;
      b2: uint8;
      b3: uint8;
      c: int16;
      d: uint16;
      e: int32;
      f: uint32;
      g: RawPtr;
      h: intptr;
      i: uintptr;
    }

    @export
    macro TestClassWithAllTypesLoadsAndStores(
        t: TestClassWithAllTypes, r: RawPtr, v1: int8, v2: uint8, v3: int16,
        v4: uint16, v5: int32, v6: uint32, v7: intptr, v8: uintptr) {
      t.a = v1;
      t.b = v2;
      t.c = v3;
      t.d = v4;
      t.e = v5;
      t.f = v6;
      t.g = r;
      t.h = v7;
      t.i = v8;
      t.a = t.a;
      t.b = t.b;
      t.c = t.c;
      t.d = t.d;
      t.e = t.e;
      t.f = t.f;
      t.g = t.g;
      t.h = t.h;
      t.i = t.i;
    }
  )");
}

TEST(Torque, TypeDeclarationOrder) {
  ExpectSuccessfulCompilation(R"(
    type Baztype = Foo | FooType;

    @abstract
    extern class Foo extends HeapObject {
      fooField: FooType;
    }

    extern class Bar extends Foo {
      barField: Bartype;
      bazfield: Baztype;
    }

    type Bartype = FooType;

    type FooType = Smi | Bar;
  )");
}

TEST(Torque, ConditionalFields) {
  // This class should throw alignment errors if @if decorators aren't
  // working.
  ExpectSuccessfulCompilation(R"(
  extern class PreprocessingTest extends HeapObject {
    @if(FALSE_FOR_TESTING) a: int8;
    @if(TRUE_FOR_TESTING) a: int16;
    b: int16;
    d: int32;
    @ifnot(TRUE_FOR_TESTING) e: int8;
    @ifnot(FALSE_FOR_TESTING) f: int16;
    g: int16;
    h: int32;
  }
  )");
  ExpectFailingCompilation(R"(
  extern class PreprocessingTest extends HeapObject {
    @if(TRUE_FOR_TESTING) a: int8;
    @if(FALSE_FOR_TESTING) a: int16;
    b: int16;
    d: int32;
    @ifnot(FALSE_FOR_TESTING) e: int8;
    @ifnot(TRUE_FOR_TESTING) f: int16;
    g: int16;
    h: int32;
  }
  )",
                           HasSubstr("aligned"));
}

TEST(Torque, ConstexprLetBindingDoesNotCrash) {
  ExpectFailingCompilation(
      R"(@export macro FooBar() { let foo = 0; check(foo >= 0); })",
      HasSubstr("Use 'const' instead of 'let' for variable 'foo'"));
}

TEST(Torque, DoubleUnderScorePrefixIllegalForIdentifiers) {
  ExpectFailingCompilation(R"(
    @export macro Foo() {
      let __x;
    }
  )",
                           HasSubstr("Lexer Error"));
}

TEST(Torque, UnusedLetBindingLintError) {
  ExpectFailingCompilation(R"(
    @export macro Foo(y: Smi) {
      let x: Smi = y;
    }
  )",
                           HasSubstr("Variable 'x' is never used."));
}

TEST(Torque, UnderscorePrefixSilencesUnusedWarning) {
  ExpectSuccessfulCompilation(R"(
    @export macro Foo(y: Smi) {
      let _x: Smi = y;
    }
  )");
}

TEST(Torque, UsingUnderscorePrefixedIdentifierError) {
  ExpectFailingCompilation(R"(
    @export macro Foo(y: Smi) {
      let _x: Smi = y;
      check(_x == y);
    }
  )",
                           HasSubstr("Trying to reference '_x'"));
}

TEST(Torque, UnusedArgumentLintError) {
  ExpectFailingCompilation(R"(
    @export macro Foo(x: Smi) {}
  )",
                           HasSubstr("Variable 'x' is never used."));
}

TEST(Torque, UsingUnderscorePrefixedArgumentSilencesWarning) {
  ExpectSuccessfulCompilation(R"(
    @export macro Foo(_y: Smi) {}
  )");
}

TEST(Torque, UnusedLabelLintError) {
  ExpectFailingCompilation(R"(
    @export macro Foo() labels Bar {}
  )",
                           HasSubstr("Label 'Bar' is never used."));
}

TEST(Torque, UsingUnderScorePrefixLabelSilencesWarning) {
  ExpectSuccessfulCompilation(R"(
    @export macro Foo() labels _Bar {}
  )");
}

TEST(Torque, NoUnusedWarningForImplicitArguments) {
  ExpectSuccessfulCompilation(R"(
    @export macro Foo(implicit c: Context, r: JSReceiver)() {}
  )");
}

TEST(Torque, NoUnusedWarningForVariablesOnlyUsedInAsserts) {
  ExpectSuccessfulCompilation(R"(
    @export macro Foo(x: bool) {
      assert(x);
    }
  )");
}

TEST(Torque, ImportNonExistentFile) {
  ExpectFailingCompilation(R"(import "foo/bar.tq")",
                           HasSubstr("File 'foo/bar.tq' not found."));
}

TEST(Torque, LetShouldBeConstLintError) {
  ExpectFailingCompilation(R"(
    @export macro Foo(y: Smi): Smi {
      let x: Smi = y;
      return x;
    })",
                           HasSubstr("Variable 'x' is never assigned to."));
}

TEST(Torque, LetShouldBeConstIsSkippedForStructs) {
  ExpectSuccessfulCompilation(R"(
    struct Foo{ a: Smi; }
    @export macro Bar(x: Smi): Foo {
      let foo = Foo{a: x};
      return foo;
    }
  )");
}

TEST(Torque, GenericAbstractType) {
  ExpectSuccessfulCompilation(R"(
    type Foo<T: type> extends HeapObject;
    extern macro F1(HeapObject);
    macro F2<T: type>(x: Foo<T>) {
      F1(x);
    }
    @export
    macro F3(a: Foo<Smi>, b: Foo<HeapObject>){
      F2(a);
      F2(b);
    }
  )");

  ExpectFailingCompilation(R"(
    type Foo<T: type> extends HeapObject;
    macro F1<T: type>(x: Foo<T>) {}
    @export
    macro F2(a: Foo<Smi>) {
      F1<HeapObject>(a);
    })",
                           HasSubstr("cannot find suitable callable"));

  ExpectFailingCompilation(R"(
    type Foo<T: type> extends HeapObject;
    extern macro F1(Foo<HeapObject>);
    @export
    macro F2(a: Foo<Smi>) {
      F1(a);
    })",
                           HasSubstr("cannot find suitable callable"));
}

TEST(Torque, SpecializationRequesters) {
  ExpectFailingCompilation(
      R"(
    macro A<T: type extends HeapObject>() {}
    macro B<T: type>() {
      A<T>();
    }
    macro C<T: type>() {
      B<T>();
    }
    macro D() {
      C<Smi>();
    }
  )",
      SubstrVector{
          SubstrTester("cannot find suitable callable", 4, 7),
          SubstrTester("Note: in specialization B<Smi> requested here", 7, 7),
          SubstrTester("Note: in specialization C<Smi> requested here", 10,
                       7)});

  ExpectFailingCompilation(
      R"(
    extern macro RetVal(): Object;
    builtin A<T: type extends HeapObject>(implicit context: Context)(): Object {
      return RetVal();
    }
    builtin B<T: type>(implicit context: Context)(): Object {
      return A<T>();
    }
    builtin C<T: type>(implicit context: Context)(): Object {
      return B<T>();
    }
    builtin D(implicit context: Context)(): Object {
      return C<Smi>();
    }
  )",
      SubstrVector{
          SubstrTester("cannot find suitable callable", 7, 14),
          SubstrTester("Note: in specialization B<Smi> requested here", 10, 14),
          SubstrTester("Note: in specialization C<Smi> requested here", 13,
                       14)});

  ExpectFailingCompilation(
      R"(
    struct A<T: type extends HeapObject> {}
    struct B<T: type> {
      a: A<T>;
    }
    struct C<T: type> {
      b: B<T>;
    }
    struct D {
      c: C<Smi>;
    }
  )",
      SubstrVector{
          SubstrTester("Could not instantiate generic", 4, 10),
          SubstrTester("Note: in specialization B<Smi> requested here", 7, 10),
          SubstrTester("Note: in specialization C<Smi> requested here", 10,
                       10)});

  ExpectFailingCompilation(
      R"(
    macro A<T: type extends HeapObject>() {}
    macro B<T: type>() {
      A<T>();
    }
    struct C<T: type> {
      Method() {
        B<T>();
      }
    }
    macro D(_b: C<Smi>) {}
  )",
      SubstrVector{
          SubstrTester("cannot find suitable callable", 4, 7),
          SubstrTester("Note: in specialization B<Smi> requested here", 8, 9),
          SubstrTester("Note: in specialization C<Smi> requested here", 11,
                       5)});
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
