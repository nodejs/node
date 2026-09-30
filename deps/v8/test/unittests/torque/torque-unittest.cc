// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <optional>
#include <sstream>

#include "src/common/globals.h"
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

type IntegerLiteral constexpr 'IntegerLiteral';

namespace torque_internal {
  struct Reference<T: type> {
    const object: HeapObject;
    const offset: intptr;
  }
  type ConstReference<T : type> extends Reference<T>;
  type MutableReference<T : type> extends ConstReference<T>;

  type UninitializedHeapObject extends HeapObject;
  macro DownCastForTorqueClass<T : type extends HeapObject>(o: HeapObject):
      T labels _CastError {
    return %RawDownCast<T>(o);
  }
  macro IsWithContext<T : type extends HeapObject>(o: HeapObject): bool {
    return false;
  }
}

namespace torque_internal {
  intrinsic %SizeOf<T: type>(): constexpr int31;
  macro TimesSizeOf<T: type>(i: intptr): intptr {
    return i * %SizeOf<T>();
  }
  struct Slice<T: type, Reference: type> {
    macro AtIndex(index: intptr): Reference {
      return unsafe::NewReference<T>(
          this.object, this.offset + TimesSizeOf<T>(index));
    }
    const object: HeapObject;
    const offset: intptr;
    const length: intptr;
  }
  namespace unsafe {
    macro NewReference<T: type>(object: HeapObject, offset: intptr): &T {
      return %RawDownCast<&T>(Reference<T>{object: object, offset: offset});
    }
    macro NewMutableSlice<T: type>(
        object: HeapObject, offset: intptr, length: intptr): MutableSlice<T> {
      return %RawDownCast<MutableSlice<T>>(
          Slice<T, &T>{object: object, offset: offset, length: length});
    }
    macro NewConstSlice<T: type>(
        object: HeapObject, offset: intptr, length: intptr): ConstSlice<T> {
      return %RawDownCast<ConstSlice<T>>(
          Slice<T, const &T>{object: object, offset: offset, length: length});
    }
  }
  intrinsic %IndexedFieldLength<T: type>(o: T, f: constexpr string): intptr;
  intrinsic %FieldSlice<T: type, TSlice: type>(o: T, f: constexpr string):
      TSlice;
}
type MutableSlice<T : type> extends torque_internal::Slice<T, &T>;
type ConstSlice<T : type> extends torque_internal::Slice<T, const &T>;

type Tagged generates 'TNode<MaybeObject>' constexpr 'MaybeObject';
type StrongTagged extends Tagged
    generates 'TNode<Object>' constexpr 'Object';
type Smi extends StrongTagged generates 'TNode<Smi>' constexpr 'Smi';
type WeakHeapObject extends Tagged;
type Weak<T : type extends HeapObject> extends WeakHeapObject;
type TaggedIndex extends StrongTagged;
type TaggedZeroPattern extends TaggedIndex;

@abstract
@doNotGenerateCppClass
extern class HeapObject extends StrongTagged {
  map: Map;
}
type Map extends HeapObject generates 'TNode<Map>';
type Object = Smi | HeapObject;
type Number = Smi|HeapNumber;
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
type uint64 generates 'TNode<UInt64T>' constexpr 'uint64_t';
type intptr generates 'TNode<IntPtrT>' constexpr 'intptr_t';
type uintptr generates 'TNode<UintPtrT>' constexpr 'uintptr_t';
type float32 generates 'TNode<Float32T>' constexpr 'float';
type float64 generates 'TNode<Float64T>' constexpr 'double';
type bool generates 'TNode<BoolT>' constexpr 'bool';
type bint generates 'TNode<BInt>' constexpr 'BInt';
type string constexpr 'const char*';
type RawPtr generates 'TNode<RawPtrT>' constexpr 'void*';
type ExternalPointer
    generates 'TNode<ExternalPointerT>' constexpr 'ExternalPointer_t';
type CppHeapPointer
    generates 'TNode<CppHeapPointerT>' constexpr 'CppHeapPointer_t';
type TrustedPointer
    generates 'TNode<TrustedPointerT>' constexpr 'TrustedPointer_t';
type ProtectedPointer extends Tagged;
type InstructionStream extends HeapObject generates 'TNode<InstructionStream>';
type BuiltinPtr extends Smi generates 'TNode<BuiltinPtr>';
type Context extends HeapObject generates 'TNode<Context>';
type NativeContext extends Context;
type SmiTagged<T : type extends uint31> extends Smi;
type String extends HeapObject;
type HeapNumber extends HeapObject;
type FixedArrayBase extends HeapObject;
type Lazy<T: type>;

struct float64_or_undefined_or_hole {
  is_undefined: bool;
  is_hole: bool;
  value: float64;
}

extern operator '+' macro IntPtrAdd(intptr, intptr): intptr;
extern operator '*' macro IntPtrMul(intptr, intptr): intptr;
extern operator '!' macro Word32BinaryNot(bool): bool;
extern operator '==' macro Word32Equal(int32, int32): bool;

intrinsic %FromConstexpr<To: type, From: type>(b: From): To;
macro Convert<To: type, From: type>(i: From): To;
extern macro SmiUntag(Smi): intptr;
Convert<intptr, Smi>(s: Smi): intptr {
  return SmiUntag(s);
}
Convert<intptr, constexpr int31>(i: constexpr int31): intptr {
  return %FromConstexpr<intptr>(i);
}
intrinsic %RawDownCast<To: type, From: type>(x: From): To;
intrinsic %RawConstexprCast<To: type, From: type>(f: From): To;
extern macro SmiConstant(constexpr Smi): Smi;
extern macro TaggedToSmi(Object): Smi
    labels CastError;
extern macro TaggedToHeapObject(Object): HeapObject
    labels CastError;
extern macro Float64SilenceNaN(float64): float64;

extern macro IntPtrConstant(constexpr int31): intptr;
extern macro ConstexprIntegerLiteralToInt32(constexpr IntegerLiteral): constexpr int32;
extern macro SmiFromInt32(int32): Smi;

macro FromConstexpr<To: type, From: type>(o: From): To;
FromConstexpr<Smi, constexpr Smi>(s: constexpr Smi): Smi {
  return SmiConstant(s);
}
FromConstexpr<Smi, constexpr int31>(s: constexpr int31): Smi {
  return %FromConstexpr<Smi>(s);
}
FromConstexpr<intptr, constexpr int31>(i: constexpr int31): intptr {
  return IntPtrConstant(i);
}
FromConstexpr<intptr, constexpr intptr>(i: constexpr intptr): intptr {
  return %FromConstexpr<intptr>(i);
}
FromConstexpr<intptr, constexpr IntegerLiteral>(
    i: constexpr IntegerLiteral): intptr {
  return %FromConstexpr<intptr>(i);
}
extern macro BoolConstant(constexpr bool): bool;
FromConstexpr<bool, constexpr bool>(b: constexpr bool): bool {
  return BoolConstant(b);
}
FromConstexpr<int32, constexpr int31>(i: constexpr int31): int32 {
  return %FromConstexpr<int32>(i);
}
FromConstexpr<int32, constexpr int32>(i: constexpr int32): int32 {
  return %FromConstexpr<int32>(i);
}
FromConstexpr<int32, constexpr IntegerLiteral>(i: constexpr IntegerLiteral): int32 {
  return FromConstexpr<int32>(ConstexprIntegerLiteralToInt32(i));
}
FromConstexpr<Smi, constexpr IntegerLiteral>(i: constexpr IntegerLiteral): Smi {
  return SmiFromInt32(FromConstexpr<int32>(i));
}

macro Cast<A : type extends Object>(implicit context: Context)(o: Object): A
    labels CastError {
  return Cast<A>(TaggedToHeapObject(o) otherwise CastError)
      otherwise CastError;
}
macro Cast<A : type extends HeapObject>(o: HeapObject): A
    labels CastError;
Cast<Smi>(o: Object): Smi
    labels CastError {
  return TaggedToSmi(o) otherwise CastError;
}
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
      std::optional<SourcePosition> actual = result.messages[i].position;
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

// TODO(almuthanna): the definition of this function is skipped on Fuchsia
// because it causes an 'unused function' exception upon buidling gn
// Ticket: https://crbug.com/1028617
#if !defined(V8_TARGET_OS_FUCHSIA)
int CountPreludeLines() {
  static int result = -1;
  if (result == -1) {
    std::string prelude(kTestTorquePrelude);
    result = static_cast<int>(std::count(prelude.begin(), prelude.end(), '\n'));
  }
  return result;
}
#endif

using SubstrWithPosition =
    std::pair<::testing::PolymorphicMatcher<
                  ::testing::internal::HasSubstrMatcher<std::string>>,
              LineAndColumn>;

// TODO(almuthanna): the definition of this function is skipped on Fuchsia
// because it causes an 'unused function' exception upon buidling gn
// Ticket: https://crbug.com/1028617
#if !defined(V8_TARGET_OS_FUCHSIA)
SubstrWithPosition SubstrTester(const std::string& message, int line, int col) {
  // Change line and column from 1-based to 0-based.
  return {::testing::HasSubstr(message),
          LineAndColumn::WithUnknownOffset(line + CountPreludeLines() - 1,
                                           col - 1)};
}
#endif

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
        v4: uint16, v5: int32, v6: uint32, v7: intptr, v8: uintptr): void {
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

// TODO(almuthanna): These tests were skipped because they cause a crash when
// they are ran on Fuchsia. This issue should be solved later on
// Ticket: https://crbug.com/1028617
#if !defined(V8_TARGET_OS_FUCHSIA)
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
      R"(@export macro FooBar(): void { let foo = 0; check(foo >= 0); })",
      HasSubstr("Use 'const' instead of 'let' for variable 'foo'"));
}

TEST(Torque, FailedImplicitCastFromConstexprDoesNotCrash) {
  ExpectFailingCompilation(
      R"(
    extern enum SomeEnum {
      kValue,
      ...
    }
    macro Foo(): void {
      Bar(SomeEnum::kValue);
    }
    macro Bar<T: type>(value: T): void {}
  )",
      HasSubstr(
          "Cannot find non-constexpr type corresponding to constexpr kValue"));
}

TEST(Torque, DoubleUnderScorePrefixIllegalForIdentifiers) {
  ExpectFailingCompilation(R"(
    @export macro Foo(): void {
      let __x;
    }
  )",
                           HasSubstr("Lexer Error"));
}
#endif

TEST(Torque, UnusedLetBindingLintError) {
  ExpectFailingCompilation(R"(
    @export macro Foo(y: Smi): void {
      let x: Smi = y;
    }
  )",
                           HasSubstr("Variable 'x' is never used."));
}

TEST(Torque, UnderscorePrefixSilencesUnusedWarning) {
  ExpectSuccessfulCompilation(R"(
    @export macro Foo(y: Smi): void {
      let _x: Smi = y;
    }
  )");
}

// TODO(almuthanna): This test was skipped because it causes a crash when it is
// ran on Fuchsia. This issue should be solved later on
// Ticket: https://crbug.com/1028617
#if !defined(V8_TARGET_OS_FUCHSIA)
TEST(Torque, UsingUnderscorePrefixedIdentifierError) {
  ExpectFailingCompilation(R"(
    @export macro Foo(y: Smi): void {
      let _x: Smi = y;
      check(_x == y);
    }
  )",
                           HasSubstr("Trying to reference '_x'"));
}
#endif

TEST(Torque, UnusedArgumentLintError) {
  ExpectFailingCompilation(R"(
    @export macro Foo(x: Smi): void {}
  )",
                           HasSubstr("Variable 'x' is never used."));
}

TEST(Torque, UsingUnderscorePrefixedArgumentSilencesWarning) {
  ExpectSuccessfulCompilation(R"(
    @export macro Foo(_y: Smi): void {}
  )");
}

TEST(Torque, UnusedLabelLintError) {
  ExpectFailingCompilation(R"(
    @export macro Foo(): void labels Bar {}
  )",
                           HasSubstr("Label 'Bar' is never used."));
}

TEST(Torque, UsingUnderScorePrefixLabelSilencesWarning) {
  ExpectSuccessfulCompilation(R"(
    @export macro Foo(): void labels _Bar {}
  )");
}

TEST(Torque, NoUnusedWarningForImplicitArguments) {
  ExpectSuccessfulCompilation(R"(
    @export macro Foo(implicit c: Context, r: JSReceiver)(): void {}
  )");
}

TEST(Torque, NoUnusedWarningForVariablesOnlyUsedInDchecks) {
  ExpectSuccessfulCompilation(R"(
    @export macro Foo(x: bool): void {
      dcheck(x);
    }
  )");
}

// TODO(almuthanna): This test was skipped because it causes a crash when it is
// ran on Fuchsia. This issue should be solved later on
// Ticket: https://crbug.com/1028617
#if !defined(V8_TARGET_OS_FUCHSIA)
TEST(Torque, ImportNonExistentFile) {
  ExpectFailingCompilation(R"(import "foo/bar.tq")",
                           HasSubstr("File 'foo/bar.tq' not found."));
}
#endif

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

// TODO(almuthanna): These tests were skipped because they cause a crash when
// they are ran on Fuchsia. This issue should be solved later on
// Ticket: https://crbug.com/1028617
#if !defined(V8_TARGET_OS_FUCHSIA)
TEST(Torque, GenericAbstractType) {
  ExpectSuccessfulCompilation(R"(
    type Foo<T: type> extends HeapObject;
    extern macro F1(HeapObject): void;
    macro F2<T: type>(x: Foo<T>): void {
      F1(x);
    }
    @export
    macro F3(a: Foo<Smi>, b: Foo<HeapObject>): void {
      F2(a);
      F2(b);
    }
  )");

  ExpectFailingCompilation(R"(
    type Foo<T: type> extends HeapObject;
    macro F1<T: type>(x: Foo<T>): void {}
    @export
    macro F2(a: Foo<Smi>): void {
      F1<HeapObject>(a);
    })",
                           HasSubstr("cannot find suitable callable"));

  ExpectFailingCompilation(R"(
    type Foo<T: type> extends HeapObject;
    extern macro F1(Foo<HeapObject>): void;
    @export
    macro F2(a: Foo<Smi>): void {
      F1(a);
    })",
                           HasSubstr("cannot find suitable callable"));
}

TEST(Torque, SpecializationRequesters) {
  ExpectFailingCompilation(
      R"(
    macro A<T: type extends HeapObject>(): void {}
    macro B<T: type>(): void {
      A<T>();
    }
    macro C<T: type>(): void {
      B<T>();
    }
    macro D(): void {
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
    macro A<T: type extends HeapObject>(): void {}
    macro B<T: type>(): void {
      A<T>();
    }
    struct C<T: type> {
      macro Method(): void {
        B<T>();
      }
    }
    macro D(_b: C<Smi>): void {}
  )",
      SubstrVector{
          SubstrTester("cannot find suitable callable", 4, 7),
          SubstrTester("Note: in specialization B<Smi> requested here", 8, 9),
          SubstrTester("Note: in specialization C<Smi> requested here", 11,
                       5)});
}
#endif

TEST(Torque, Enums) {
  ExpectSuccessfulCompilation(R"(
    extern enum MyEnum {
      kValue0,
      kValue1,
      @sameEnumValueAs(kValue0) kValue2,
      kValue3
    }
  )");

  ExpectFailingCompilation(R"(
    extern enum MyEmptyEnum {
    }
  )",
                           HasSubstr("unexpected token \"}\""));
}

TEST(Torque, EnumInTypeswitch) {
  ExpectSuccessfulCompilation(R"(
    extern enum MyEnum extends Smi {
      kA,
      kB,
      kC
    }

    @export
    macro Test(implicit context: Context)(v : MyEnum): Smi {
      typeswitch(v) {
        case (MyEnum::kA | MyEnum::kB): {
          return 1;
        }
        case (MyEnum::kC): {
          return 2;
        }
      }
    }
  )");

  ExpectSuccessfulCompilation(R"(
    extern enum MyEnum extends Smi {
      kA,
      kB,
      kC,
      ...
    }

    @export
    macro Test(implicit context: Context)(v : MyEnum): Smi {
      typeswitch(v) {
         case (MyEnum::kC): {
          return 2;
        }
        case (MyEnum::kA | MyEnum::kB): {
          return 1;
        }
       case (MyEnum): {
          return 0;
        }
      }
    }
  )");

  ExpectSuccessfulCompilation(R"(
  extern enum MyEnum extends Smi {
    kA,
    kB,
    kC,
    ...
  }

  @export
  macro Test(implicit context: Context)(b: bool): Smi {
    return b ? MyEnum::kB : MyEnum::kA;
  }
)");
}

TEST(Torque, EnumTypeAnnotations) {
  ExpectSuccessfulCompilation(R"(
    type Type1 extends intptr;
    type Type2 extends intptr;
    extern enum MyEnum extends intptr {
      kValue1: Type1,
      kValue2: Type2,
      kValue3
    }
    @export macro Foo(): void {
      const _a: Type1 = MyEnum::kValue1;
      const _b: Type2 = MyEnum::kValue2;
      const _c: intptr = MyEnum::kValue3;
    }
  )");
}

TEST(Torque, ConstClassFields) {
  ExpectSuccessfulCompilation(R"(
    class Foo extends HeapObject {
      const x: int32;
      y: int32;
    }

    @export
    macro Test(implicit context: Context)(o: Foo, n: int32): void {
      const _x: int32 = o.x;
      o.y = n;
    }
  )");

  ExpectFailingCompilation(R"(
    class Foo extends HeapObject {
      const x: int32;
    }

    @export
    macro Test(implicit context: Context)(o: Foo, n: int32): void {
      o.x = n;
    }
  )",
                           HasSubstr("cannot assign to const value"));

  ExpectSuccessfulCompilation(R"(
    class Foo extends HeapObject {
      s: Bar;
    }
    struct Bar {
      const x: int32;
      y: int32;
    }

    @export
    macro Test(implicit context: Context)(o: Foo, n: int32): void {
      const _x: int32 = o.s.x;
      // Assigning a struct as a value is OK, even when the struct contains
      // const fields.
      o.s = Bar{x: n, y: n};
      o.s.y = n;
    }
  )");

  ExpectFailingCompilation(R"(
    class Foo extends HeapObject {
      const s: Bar;
    }
    struct Bar {
      const x: int32;
      y: int32;
    }

    @export
    macro Test(implicit context: Context)(o: Foo, n: int32): void {
      o.s.y = n;
    }
  )",
                           HasSubstr("cannot assign to const value"));

  ExpectFailingCompilation(R"(
    class Foo extends HeapObject {
      s: Bar;
    }
    struct Bar {
      const x: int32;
      y: int32;
    }

    @export
    macro Test(implicit context: Context)(o: Foo, n: int32): void {
      o.s.x = n;
    }
  )",
                           HasSubstr("cannot assign to const value"));
}

TEST(Torque, References) {
  ExpectSuccessfulCompilation(R"(
    class Foo extends HeapObject {
      const x: int32;
      y: int32;
    }

    @export
    macro Test(implicit context: Context)(o: Foo, n: int32): void {
      const constRefX: const &int32 = &o.x;
      const refY: &int32 = &o.y;
      const constRefY: const &int32 = refY;
      const _x: int32 = *constRefX;
      const _y1: int32 = *refY;
      const _y2: int32 = *constRefY;
      *refY = n;
      let r: const &int32 = constRefX;
      r = constRefY;
    }
  )");

  ExpectFailingCompilation(R"(
    class Foo extends HeapObject {
      const x: int32;
      y: int32;
    }

    @export
    macro Test(implicit context: Context)(o: Foo): void {
      const _refX: &int32 = &o.x;
    }
  )",
                           HasSubstr("cannot use expression of type const "
                                     "&int32 as a value of type &int32"));

  ExpectFailingCompilation(R"(
    class Foo extends HeapObject {
      const x: int32;
      y: int32;
    }

    @export
    macro Test(implicit context: Context)(o: Foo, n: int32): void {
      const constRefX: const &int32 = &o.x;
      *constRefX = n;
    }
  )",
                           HasSubstr("cannot assign to const value"));
}

TEST(Torque, CatchFirstHandler) {
  ExpectFailingCompilation(
      R"(
    @export
    macro Test(): void {
      try {
      } label Foo {
      } catch (_e, _m) {}
    }
  )",
      HasSubstr(
          "catch handler always has to be first, before any label handler"));
}

TEST(Torque, BitFieldLogicalAnd) {
  std::string prelude = R"(
    bitfield struct S extends uint32 {
      a: bool: 1 bit;
      b: bool: 1 bit;
      c: int32: 5 bit;
    }
    macro Test(s: S): bool { return
  )";
  std::string postlude = ";}";
  std::string message = "use & rather than &&";
  ExpectFailingCompilation(prelude + "s.a && s.b" + postlude,
                           HasSubstr(message));
  ExpectFailingCompilation(prelude + "s.a && !s.b" + postlude,
                           HasSubstr(message));
  ExpectFailingCompilation(prelude + "!s.b && s.c == 34" + postlude,
                           HasSubstr(message));
}

TEST(Torque, FieldAccessOnNonClassType) {
  ExpectFailingCompilation(
      R"(
    @export
    macro Test(x: Number): Map {
      return x.map;
    }
  )",
      HasSubstr("map"));
}

TEST(Torque, UnusedImplicit) {
  ExpectSuccessfulCompilation(R"(
    @export
    macro Test1(implicit c: Smi)(a: Object): Object { return a; }
    @export
    macro Test2(b: Object): void { Test1(b);  }
  )");

  ExpectFailingCompilation(
      R"(
    macro Test1(implicit c: Smi)(_a: Object): Smi { return c; }
    @export
    macro Test2(b: Smi): void { Test1(b);  }
  )",
      HasSubstr("undefined expression of type Smi: the implicit "
                "parameter 'c' is not defined when invoking Test1 at"));

  ExpectFailingCompilation(
      R"(
    extern macro Test3(implicit c: Smi)(Object): Smi;
    @export
    macro Test4(b: Smi): void { Test3(b);  }
  )",
      HasSubstr("unititialized implicit parameters can only be passed to "
                "Torque-defined macros: the implicit parameter 'c' is not "
                "defined when invoking Test3"));
  ExpectSuccessfulCompilation(
      R"(
    macro Test7<T: type>(implicit c: Smi)(o: T): Smi;
    Test7<Smi>(implicit c: Smi)(o: Smi): Smi { return o; }
    @export
    macro Test8(b: Smi): void { Test7(b); }
  )");

  ExpectFailingCompilation(
      R"(
    macro Test6<T: type>(_o: T): T;
    macro Test6<T: type>(implicit c: T)(_o: T): T {
      return c;
    }
    macro Test7<T: type>(o: T): Smi;
    Test7<Smi>(o: Smi): Smi { return Test6<Smi>(o); }
    @export
    macro Test8(b: Smi): void { Test7(b); }
  )",
      HasSubstr("\nambiguous callable : \n  Test6(Smi)\ncandidates are:\n  "
                "Test6(Smi): Smi\n  Test6(implicit Smi)(Smi): Smi"));
}

TEST(Torque, ImplicitTemplateParameterInference) {
  ExpectSuccessfulCompilation(R"(
    macro Foo(_x: Map): void {}
    macro Foo(_x: Smi): void {}
    macro GenericMacro<T: type>(implicit x: T)(): void {
      Foo(x);
    }
    @export
    macro Test1(implicit x: Smi)(): void { GenericMacro(); }
    @export
    macro Test2(implicit x: Map)(): void { GenericMacro();  }
  )");

  ExpectFailingCompilation(
      R"(
    // Wrap in namespace to avoid redeclaration error.
    namespace foo {
    macro Foo(implicit x: Map)(): void {}
    }
    macro Foo(implicit x: Smi)(): void {}
    namespace foo{
    @export
    macro Test(implicit x: Smi)(): void { Foo(); }
    }
  )",
      HasSubstr("ambiguous callable"));

  ExpectFailingCompilation(
      R"(
    // Wrap in namespace to avoid redeclaration error.
    namespace foo {
    macro Foo(implicit x: Map)(): void {}
    }
    macro Foo(implicit x: Smi)(): void {}
    namespace foo{
    @export
    macro Test(implicit x: Map)(): void { Foo(); }
    }
  )",
      HasSubstr("ambiguous callable"));
}

TEST(Torque, BuiltinReturnsNever) {
  ExpectFailingCompilation(
      "builtin Never(): never {}",
      HasSubstr("control reaches end of builtin, expected return of a value"));
  ExpectFailingCompilation(
      "builtin Never(): never { return 1; }",
      HasSubstr("cannot return from a function with return type never"));
  ExpectFailingCompilation(
      R"(
    extern macro Throw(): never;
    builtin Never(): never {
      Throw();
    }
    builtin CallsNever(): Smi {
      Never();
      return 1;
    }
  )",
      HasSubstr("statement after non-returning statement"));

  ExpectSuccessfulCompilation(
      "extern macro Throw(): never;"
      "builtin Never(): never { Throw(); }");
  ExpectSuccessfulCompilation(R"(
    extern macro Throw(): never;
    builtin Never(implicit c: Context, a: int32)(): never {
      if(a == 1) {
        Throw();
      } else {
        Throw();
      }
    }
  )");
}

namespace {

// The build configuration every layout JSON carries. The loader checks it
// against TargetArchitecture, which under the test options resolves to
// the host build's sizes.
std::string TestConfig() {
  std::stringstream s;
  s << R"("config": {"tagged_size": )" << kTaggedSize << R"(, "pointer_size": )"
    << kSystemPointerSize << R"(, "external_pointer_size": )"
    << kExternalPointerSlotSize << R"(, "cpp_heap_pointer_size": )"
    << kCppHeapPointerSlotSize << R"(, "trusted_pointer_size": )"
    << kTrustedPointerSize << R"(}, )";
  return s.str();
}

// A class with its layout defined in C++, as the layout JSON and the
// loader see it: two tagged fields a and b on top of the prelude's
// HeapObject.
constexpr const char* kTestLayoutClass = R"(
  @cppObjectLayoutDefinition
  extern class TestLayout extends HeapObject {
    a: Smi;
    b: Map;
  }
)";

constexpr const char* kTestLayoutBodyLessClass = R"(
  @cppObjectLayoutDefinition
  extern class TestLayout extends HeapObject;
)";

// The layout JSON record matching kTestLayoutClass, with the variations
// the failure tests need.
std::string TestLayoutJson(int schema_version, size_t offset_b_delta = 0,
                           const char* type_b = "Map",
                           const char* type_a = "Smi",
                           const char* type_override_a = nullptr) {
  // What TargetArchitecture::TaggedSize() resolves to under the
  // force_32bit_output=false options every test here compiles with.
  size_t t = kTaggedSize;
  std::stringstream s;
  s << R"({"schema_version": )" << schema_version << R"(, )" << TestConfig()
    << R"("classes": [{)"
    << R"("cpp_name": "v8::internal::TestLayout", )"
    << R"("base": "v8::internal::HeapObject", )"
    << R"("base_size": )" << t << R"(, "size": )" << 3 * t
    << R"(, "alignment": )" << t << R"(, "fields": [)"
    << R"({"cpp_name": "a_", "cpp_type": "TaggedMember<Object>", "offset": )"
    << t << R"(, "size": )" << t << R"(, "storage": {"kind": "tagged", "arg": )"
    << R"({"kind": "name", "name": ")" << type_a << R"("}})";
  if (type_override_a != nullptr) {
    s << R"(, "annotations": [{"name": "V8_TQ_TYPE", "arg": ")"
      << type_override_a << R"("}])";
  }
  s << R"(}, )"
    << R"({"cpp_name": "b_", "cpp_type": "TaggedMember<Object>", "offset": )"
    << 2 * t + offset_b_delta << R"(, "size": )" << t
    << R"(, "storage": {"kind": "tagged", "arg": )"
    << R"({"kind": "name", "name": ")" << type_b << R"("}}})"
    << R"(]}]})";
  return s.str();
}

TorqueCompilerResult TestCompileTorqueWithLayout(
    std::string source, const std::string& layout_json, bool use_cpp_layouts,
    const char* positions = nullptr) {
  // Parallel test runners share TempDir; the test name keys the file.
  std::string path =
      ::testing::TempDir() + "/torque-unittest-layout-" +
      ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".json";
  {
    std::ofstream out(path);
    out << layout_json;
  }

  TorqueCompilerOptions options;
  options.output_directory = "";
  options.collect_language_server_data = false;
  options.force_assert_statements = false;
  options.v8_root = ".";
  options.layout_json_path = path;
  options.use_cpp_layouts = use_cpp_layouts;
  if (positions != nullptr) {
    std::string positions_path = path + "-positions.json";
    std::ofstream out(positions_path);
    out << positions;
    options.layout_positions_path = positions_path;
  }

  source = kTestTorquePrelude + source;
  return CompileTorque(source, options);
}

void ExpectSuccessfulLayoutCompilation(const std::string& source,
                                       const std::string& layout_json,
                                       bool use_cpp_layouts) {
  TorqueCompilerResult result =
      TestCompileTorqueWithLayout(source, layout_json, use_cpp_layouts);
  std::vector<std::string> messages;
  for (const auto& message : result.messages) {
    messages.push_back(message.message);
  }
  EXPECT_EQ(messages, std::vector<std::string>{});
}

void ExpectFailingLayoutCompilation(const std::string& source,
                                    const std::string& layout_json,
                                    bool use_cpp_layouts,
                                    const std::string& expected_substring) {
  TorqueCompilerResult result =
      TestCompileTorqueWithLayout(source, layout_json, use_cpp_layouts);
  ASSERT_FALSE(result.messages.empty());
  EXPECT_THAT(result.messages.front().message, HasSubstr(expected_substring));
}

}  // namespace

TEST(TorqueLayoutLoader, VerifierAcceptsMatchingRecord) {
  ExpectSuccessfulLayoutCompilation(kTestLayoutClass, TestLayoutJson(1),
                                    /*use_cpp_layouts=*/false);
}

TEST(TorqueLayoutLoader, VerifierRejectsOffsetMismatch) {
  ExpectFailingLayoutCompilation(kTestLayoutClass,
                                 TestLayoutJson(1, /*offset_b_delta=*/4),
                                 /*use_cpp_layouts=*/false, "offset is");
}

TEST(TorqueLayoutLoader, SchemaVersionMismatchAborts) {
  ExpectFailingLayoutCompilation(kTestLayoutClass, TestLayoutJson(99),
                                 /*use_cpp_layouts=*/false,
                                 "unsupported schema version");
}

TEST(TorqueLayoutLoader, RejectsAMismatchedBuildConfiguration) {
  // A layout JSON generated for another configuration describes offsets
  // this Torque run does not target. While the .tq field blocks exist a
  // mismatch also shows up as a field mismatch; once they are gone this
  // check is all that catches a stale layout JSON.
  std::string layout_json = TestLayoutJson(1);
  std::string tagged = R"("tagged_size": )" + std::to_string(kTaggedSize);
  size_t pos = layout_json.find(tagged);
  ASSERT_NE(pos, std::string::npos);
  layout_json.replace(pos, tagged.size(),
                      R"("tagged_size": )" + std::to_string(kTaggedSize * 2));
  ExpectFailingLayoutCompilation(kTestLayoutClass, layout_json,
                                 /*use_cpp_layouts=*/false,
                                 "does not match this build configuration");
}

TEST(TorqueLayoutLoader, RejectsALayoutJsonWithoutAConfig) {
  std::string layout_json = TestLayoutJson(1);
  size_t start = layout_json.find(R"("config": {)");
  size_t end = layout_json.find(R"("classes")");
  ASSERT_NE(start, std::string::npos);
  ASSERT_LT(start, end);
  layout_json.erase(start, end - start);
  ExpectFailingLayoutCompilation(kTestLayoutClass, layout_json,
                                 /*use_cpp_layouts=*/false,
                                 "missing key \"config\"");
}

TEST(TorqueLayoutLoader, RecordsWithoutConsumerAreSkipped) {
  // No TestLayout class in the source at all; the record is unconsumed.
  ExpectSuccessfulLayoutCompilation("", TestLayoutJson(1),
                                    /*use_cpp_layouts=*/false);
}

TEST(TorqueLayoutLoader, ImporterImportsBodyLessClass) {
  // Import builds the fields from the record; the verifier then
  // cross-checks the computed layout against the same record.
  ExpectSuccessfulLayoutCompilation(kTestLayoutBodyLessClass, TestLayoutJson(1),
                                    /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, ImporterRequiresRecordForBodyLessClass) {
  ExpectFailingLayoutCompilation(
      kTestLayoutBodyLessClass,
      (R"({"schema_version": 1, )" + TestConfig() + R"("classes": []})"),
      /*use_cpp_layouts=*/true, "has no layout record");
}

TEST(TorqueLayoutLoader, ImporterReplacesMatchingTqFieldBlock) {
  ExpectSuccessfulLayoutCompilation(kTestLayoutClass, TestLayoutJson(1),
                                    /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, ClassTemplateArgumentIsTypedByItsBase) {
  // A class template in the layout JSON maps to the base class of its
  // instantiations, since Torque has no generic class types.
  constexpr const char* kSource = R"(
    extern class CppGCManagedBase extends HeapObject;
    @cppObjectLayoutDefinition
    extern class TestLayout extends HeapObject {
      a: Smi;
      b: CppGCManagedBase;
    }
  )";
  ExpectSuccessfulLayoutCompilation(
      kSource, TestLayoutJson(1, 0, /*type_b=*/"CppGCManaged"),
      /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, ImporterRejectsMismatchedTqFieldBlock) {
  // The record has b: Smi, the .tq field block b: Map. The strict
  // comparison must fail rather than silently replace the block.
  ExpectFailingLayoutCompilation(
      kTestLayoutClass, TestLayoutJson(1, 0, /*type_b=*/"Smi"),
      /*use_cpp_layouts=*/true, "differs from C++ field");
}

TEST(TorqueLayoutLoader, ImporterReportsEveryMismatchedField) {
  // Both fields differ; the comparison reports each one instead of
  // stopping at the first.
  TorqueCompilerResult result = TestCompileTorqueWithLayout(
      kTestLayoutClass,
      TestLayoutJson(1, /*offset_b_delta=*/0, /*type_b=*/"Smi",
                     /*type_a=*/"HeapObject"),
      /*use_cpp_layouts=*/true);
  std::vector<std::string> mismatches;
  for (const auto& message : result.messages) {
    if (message.message.find("differs from C++ field") != std::string::npos) {
      mismatches.push_back(message.message);
    }
  }
  ASSERT_EQ(mismatches.size(), 2u);
  EXPECT_THAT(mismatches[0], HasSubstr("a: Smi"));
  EXPECT_THAT(mismatches[0], HasSubstr("a: HeapObject"));
  EXPECT_THAT(mismatches[1], HasSubstr("b: Map"));
  EXPECT_THAT(mismatches[1], HasSubstr("b: Smi"));
}

namespace {

// The tail exercises the flexible-array path of the verifier and the
// importer; the length field indexes it.
constexpr const char* kTestLayoutTailClass = R"(
  @cppObjectLayoutDefinition
  extern class TestBlob extends HeapObject {
    const length: Smi;
    bytes[length]: uint8;
  }
)";

constexpr const char* kTestLayoutTailBodyLessClass = R"(
  @cppObjectLayoutDefinition
  extern class TestBlob extends HeapObject;
)";

std::string TestTailJson(const char* length_field = "length") {
  size_t t = kTaggedSize;
  std::stringstream s;
  s << R"({"schema_version": 1, )" << TestConfig() << R"("classes": [{)"
    << R"("cpp_name": "v8::internal::TestBlob", )"
    << R"("base": "v8::internal::HeapObject", )"
    << R"("base_size": )" << t << R"(, "size": )" << 2 * t
    << R"(, "alignment": )" << t << R"(, "fields": [)"
    << R"({"cpp_name": "length_", "cpp_type": "TaggedMember<Smi>", "offset": )"
    << t << R"(, "size": )" << t << R"(, "storage": {"kind": "tagged", "arg": )"
    << R"({"kind": "name", "name": "Smi"}}, )"
    << R"("annotations": [{"name": "V8_TQ_CONST"}]}], )"
    << R"("tail": {"cpp_name": "flexible_array_member_data_", )"
    << R"("cpp_element_type": "uint8_t", )"
    << R"("offset": )" << 2 * t << R"(, "element_size": 1, )"
    << R"("element_storage": {"kind": "int", "width": 1, "signed": false}}, )"
    << R"("annotations": [{"name": "V8_TQ_TAIL_NAME", "arg": "bytes"}, )"
    << R"({"name": "V8_TQ_TAIL_LENGTH", "arg": ")" << length_field
    << R"("}]}]})";
  return s.str();
}

// A named-constant array extent cannot be folded into a static class
// size; the record carries both the numeric extent and the Torque
// constant name.
constexpr const char* kTestLayoutNamedExtentClass = R"(
  const kTestExtent: constexpr int31 generates '2';
  @cppObjectLayoutDefinition
  extern class TestExtent extends HeapObject {
    a[kTestExtent]: Smi;
  }
)";

std::string TestNamedExtentJson() {
  size_t t = kTaggedSize;
  std::stringstream s;
  s << R"({"schema_version": 1, )" << TestConfig() << R"("classes": [{)"
    << R"("cpp_name": "v8::internal::TestExtent", )"
    << R"("base": "v8::internal::HeapObject", )"
    << R"("base_size": )" << t << R"(, "size": )" << 3 * t
    << R"(, "alignment": )" << t << R"(, "fields": [)"
    << R"({"cpp_name": "a_", "cpp_type": "TaggedMember<Object>", "offset": )"
    << t << R"(, "size": )" << 2 * t
    << R"(, "storage": {"kind": "tagged", "arg": )"
    << R"({"kind": "name", "name": "Smi"}}, "array_extent": 2, )"
    << R"("annotations": [{"name": "V8_TQ_EXTENT_NAME", )"
    << R"("arg": "kTestExtent"}]}]}]})";
  return s.str();
}

// kTestLayoutClass with field a declared as the override target.
constexpr const char* kTestLayoutOverrideClass = R"(
  @cppObjectLayoutDefinition
  extern class TestLayout extends HeapObject {
    a: SmiTagged<uint31>;
    b: Map;
  }
)";

// One scalar field on top of HeapObject. The storage record is the
// only input the Torque type is derived from, so declaring the class
// with the expected type turns the strict comparison into an assertion
// on the derivation.
std::string TestScalarClass(const std::string& field) {
  // "T" names a field called value of type T; "n: T" names it n.
  std::string spelled =
      field.find(':') == std::string::npos ? "value: " + field : field;
  return "@cppObjectLayoutDefinition\nextern class TestScalar extends "
         "HeapObject {\n  " +
         spelled + ";\n}\n";
}

std::string TestScalarJson(const std::string& storage, size_t size,
                           const char* annotations = nullptr) {
  size_t t = kTaggedSize;
  std::stringstream s;
  s << R"({"schema_version": 1, )" << TestConfig() << R"("classes": [{)"
    << R"("cpp_name": "v8::internal::TestScalar", )"
    << R"("base": "v8::internal::HeapObject", )"
    << R"("base_size": )" << t << R"(, "size": )" << t + size
    << R"(, "alignment": )" << t << R"(, "fields": [)"
    << R"({"cpp_name": "value_", "offset": )" << t << R"(, "size": )" << size
    << R"(, "storage": )" << storage;
  if (annotations != nullptr) s << R"(, "annotations": )" << annotations;
  s << R"(}]}]})";
  return s.str();
}

// The positions JSON for kTestLayoutClass and its variants.
constexpr const char* kTestLayoutPositions = R"({"schema_version": 1,
  "classes": [{"cpp_name": "v8::internal::TestLayout",
    "position": "src/objects/test-layout.h:10:1",
    "members": {"a_": "src/objects/test-layout.h:11:3",
                "b_": "src/objects/test-layout.h:12:3"}}]})";

}  // namespace

// The same layout, with the class omitting its tail (V8_TQ_NO_TAIL)
// instead of naming it: only the fixed header becomes Torque fields.
std::string TestNoTailJson(bool also_name_the_tail = false) {
  std::string document = TestTailJson();
  size_t annotations =
      document.find(R"("annotations": [{"name": "V8_TQ_TAIL_NAME")");
  CHECK_NE(annotations, std::string::npos);
  std::string replacement = R"("annotations": [{"name": "V8_TQ_NO_TAIL"})";
  if (also_name_the_tail) {
    replacement += R"(, {"name": "V8_TQ_TAIL_NAME", "arg": "bytes"})"
                   R"(, {"name": "V8_TQ_TAIL_LENGTH", "arg": "length"})";
  }
  replacement += "]}]}";
  return document.substr(0, annotations) + replacement;
}

constexpr const char* kTestLayoutNoTailClass = R"(
  @cppObjectLayoutDefinition
  extern class TestBlob extends HeapObject {
    const length: Smi;
  }
)";

// A tail Torque splits into two indexed sections: the section
// declarations are on the class as Torque source, because their extents
// are expressions over Torque fields that C++ does not encode. Optional
// sections (`name?[cond]`) take the same path; the test prelude has no
// Smi comparison to write a condition with, so ScopeInfo covers them in
// the real build.
std::string TestSectionedJson(const char* sections, bool also_no_tail) {
  std::string document = TestTailJson();
  size_t annotations =
      document.find(R"("annotations": [{"name": "V8_TQ_TAIL_NAME")");
  CHECK_NE(annotations, std::string::npos);
  std::string replacement =
      std::string(
          R"("annotations": [{"name": "V8_TQ_TAIL_SECTIONS", "arg": ")") +
      sections + R"("})";
  if (also_no_tail) replacement += R"(, {"name": "V8_TQ_NO_TAIL"})";
  return document.substr(0, annotations) + replacement + "]}]}";
}

constexpr const char* kTestLayoutSectionedClass = R"(
  @cppObjectLayoutDefinition
  extern class TestBlob extends HeapObject {
    const length: Smi;
    head[length]: uint8;
    rest[length]: uint8;
  }
)";

TEST(TorqueLayoutLoader, ImportsASectionedTail) {
  ExpectSuccessfulLayoutCompilation(
      kTestLayoutSectionedClass,
      TestSectionedJson("head[length]: uint8; rest[length]: uint8;",
                        /*also_no_tail=*/false),
      /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, RejectsAComplexExtentThatDiffers) {
  // A compound extent must compare on its structure. Mapping every
  // unsupported expression to one placeholder would let these two match.
  ExpectFailingLayoutCompilation(
      R"(
  @cppObjectLayoutDefinition
  extern class TestBlob extends HeapObject {
    const length: Smi;
    head[Convert<intptr>(length) * 2]: uint8;
    rest[length]: uint8;
  }
)",
      TestSectionedJson(
          "head[Convert<intptr>(length) * 4]: uint8; rest[length]: uint8;",
          /*also_no_tail=*/false),
      /*use_cpp_layouts=*/true, "(Convert<intptr>(length) * 4)");
}

TEST(TorqueLayoutLoader, RejectsSectionsThatDifferFromTheTqFieldBlock) {
  ExpectFailingLayoutCompilation(
      kTestLayoutSectionedClass,
      TestSectionedJson("head[length]: uint8; rest[capacity]: uint8;",
                        /*also_no_tail=*/false),
      /*use_cpp_layouts=*/true, "differs from C++ field");
}

TEST(TorqueLayoutLoader, RejectsMalformedTailSections) {
  ExpectFailingLayoutCompilation(kTestLayoutSectionedClass,
                                 TestSectionedJson("head[length] uint8;",
                                                   /*also_no_tail=*/false),
                                 /*use_cpp_layouts=*/true, "unexpected token");
}

TEST(TorqueLayoutLoader, RejectsSectionsCombinedWithNoTail) {
  ExpectFailingLayoutCompilation(
      kTestLayoutSectionedClass,
      TestSectionedJson("head[length]: uint8; rest[length]: uint8;",
                        /*also_no_tail=*/true),
      /*use_cpp_layouts=*/true,
      "V8_TQ_TAIL_NAME, V8_TQ_NO_TAIL and V8_TQ_TAIL_SECTIONS are exclusive");
}

TEST(TorqueLayoutLoader, ImportsFixedHeaderWithNoTail) {
  // V8_TQ_NO_TAIL: the C++ class has a flexible array member Torque
  // does not model, so the fields stop at the fixed header.
  ExpectSuccessfulLayoutCompilation(kTestLayoutNoTailClass, TestNoTailJson(),
                                    /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, RejectsATailThatIsBothNamedAndOmitted) {
  ExpectFailingLayoutCompilation(kTestLayoutNoTailClass,
                                 TestNoTailJson(/*also_name_the_tail=*/true),
                                 /*use_cpp_layouts=*/true,
                                 "V8_TQ_TAIL_NAME, V8_TQ_NO_TAIL and "
                                 "V8_TQ_TAIL_SECTIONS are exclusive");
}

TEST(TorqueLayoutLoader, OmittedTailStillRejectsAnIndexedField) {
  // The tail contributes no field, so a Torque class that does declare
  // one no longer matches the record.
  ExpectFailingLayoutCompilation(kTestLayoutTailClass, TestNoTailJson(),
                                 /*use_cpp_layouts=*/true,
                                 ".tq declares 2 field(s), C++ 1");
}

TEST(TorqueLayoutLoader, VerifierAcceptsTailedRecord) {
  ExpectSuccessfulLayoutCompilation(kTestLayoutTailClass, TestTailJson(),
                                    /*use_cpp_layouts=*/false);
}

TEST(TorqueLayoutLoader, VerifierRejectsTailIndexMismatch) {
  ExpectFailingLayoutCompilation(kTestLayoutTailClass, TestTailJson("other"),
                                 /*use_cpp_layouts=*/false, "indexed by");
}

TEST(TorqueLayoutLoader, ImporterImportsTailedBodyLessClass) {
  // Import builds the indexed field from the tail record; the
  // verifier then cross-checks element size, index, and header size.
  ExpectSuccessfulLayoutCompilation(kTestLayoutTailBodyLessClass,
                                    TestTailJson(),
                                    /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, ImporterAppliesTypeOverride) {
  // The type derived from the record's storage is Smi; only the
  // V8_TQ_TYPE override makes the strict comparison pass.
  ExpectSuccessfulLayoutCompilation(
      kTestLayoutOverrideClass,
      TestLayoutJson(1, 0, "Map", "Smi",
                     /*type_override_a=*/"SmiTagged<uint31>"),
      /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, ImporterRejectsMalformedTypeOverride) {
  // The grammar reports the parse failure itself; with the positions
  // JSON the error points at the annotation's C++ header.
  TorqueCompilerResult result = TestCompileTorqueWithLayout(
      kTestLayoutOverrideClass,
      TestLayoutJson(1, 0, "Map", "Smi", /*type_override_a=*/"Smi |"),
      /*use_cpp_layouts=*/true, kTestLayoutPositions);
  ASSERT_FALSE(result.messages.empty());
  const TorqueMessage& message = result.messages.front();
  EXPECT_THAT(message.message, HasSubstr("unexpected end of input"));
  ASSERT_TRUE(message.position.has_value());
  SourceFileMap::Scope source_map_scope(*result.source_file_map);
  EXPECT_EQ(SourceFileMap::PathFromV8Root(message.position->source),
            "src/objects/test-layout.h");
}

TEST(TorqueLayoutLoader, ImporterFallsBackToClassPosition) {
  // Without the positions JSON the same error points at the .tq class
  // declaration.
  TorqueCompilerResult result = TestCompileTorqueWithLayout(
      kTestLayoutOverrideClass,
      TestLayoutJson(1, 0, "Map", "Smi", /*type_override_a=*/"Smi |"),
      /*use_cpp_layouts=*/true);
  ASSERT_FALSE(result.messages.empty());
  const TorqueMessage& message = result.messages.front();
  EXPECT_THAT(message.message, HasSubstr("unexpected end of input"));
  ASSERT_TRUE(message.position.has_value());
  SourceFileMap::Scope source_map_scope(*result.source_file_map);
  EXPECT_THAT(SourceFileMap::PathFromV8Root(message.position->source),
              ::testing::EndsWith(".tq"));
}

TEST(TorqueLayoutLoader, DerivesIntTypeFromWidthAndSign) {
  ExpectSuccessfulLayoutCompilation(
      TestScalarClass("uint32"),
      TestScalarJson(R"({"kind": "int", "width": 4, "signed": false})", 4),
      /*use_cpp_layouts=*/true);
  ExpectSuccessfulLayoutCompilation(
      TestScalarClass("int32"),
      TestScalarJson(R"({"kind": "int", "width": 4, "signed": true})", 4),
      /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, DerivesPointerWidthAliasAsUintptr) {
  // A member written through uintptr_t: the layout JSON names the C++
  // alias, and the loader maps it to a Torque type. The width alone
  // would give uint64 here and uint32 on a 32-bit target.
  ExpectSuccessfulLayoutCompilation(
      TestScalarClass("uintptr"),
      TestScalarJson(
          R"({"kind": "int", "width": )" + std::to_string(kSystemPointerSize) +
              R"(, "signed": false, "pointer_width_alias": "uintptr_t"})",
          kSystemPointerSize),
      /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, PointerSizedIntIsNotItsFixedWidthType) {
  // The same storage without the alias would derive uint64, so
  // declaring uint64 must be rejected.
  ExpectFailingLayoutCompilation(
      TestScalarClass("uint64"),
      TestScalarJson(
          R"({"kind": "int", "width": )" + std::to_string(kSystemPointerSize) +
              R"(, "signed": false, "pointer_width_alias": "uintptr_t"})",
          kSystemPointerSize),
      /*use_cpp_layouts=*/true, "differs from C++ field");
}

TEST(TorqueLayoutLoader, DerivesVoidForZeroSizedField) {
  // The zero-length-array padding idiom.
  ExpectSuccessfulLayoutCompilation(
      TestScalarClass("void"),
      TestScalarJson(R"({"kind": "int", "width": 1, "signed": true})", 0),
      /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, DerivesNamedTypeForEnumStorage) {
  ExpectSuccessfulLayoutCompilation(
      TestScalarClass("uint8"),
      TestScalarJson(
          R"({"kind": "enum", "name": "uint8", "width": 1, "signed": false})",
          1),
      /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, DerivesWrappedPointerTypes) {
  ExpectSuccessfulLayoutCompilation(
      TestScalarClass("ExternalPointer"),
      TestScalarJson(R"({"kind": "external_pointer", "tag": "kTestTag"})",
                     kExternalPointerSlotSize),
      /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, RejectsUnknownStorageKind) {
  ExpectFailingLayoutCompilation(
      TestScalarClass("uint32"), TestScalarJson(R"({"kind": "quantum"})", 4),
      /*use_cpp_layouts=*/true, "unknown storage kind");
}

TEST(TorqueLayoutLoader, RejectsUnknownAnnotation) {
  ExpectFailingLayoutCompilation(
      TestScalarClass("uint32"),
      TestScalarJson(R"({"kind": "int", "width": 4, "signed": false})", 4,
                     R"([{"name": "V8_TQ_BOGUS"}])"),
      /*use_cpp_layouts=*/true, "unknown annotation V8_TQ_BOGUS");
}

TEST(TorqueLayoutLoader, RejectsAnnotationWithWrongArity) {
  ExpectFailingLayoutCompilation(
      TestScalarClass("uint32"),
      TestScalarJson(R"({"kind": "int", "width": 4, "signed": false})", 4,
                     R"([{"name": "V8_TQ_CONST", "arg": "x"}])"),
      /*use_cpp_layouts=*/true, "V8_TQ_CONST takes no argument");
  ExpectFailingLayoutCompilation(
      TestScalarClass("uint32"),
      TestScalarJson(R"({"kind": "int", "width": 4, "signed": false})", 4,
                     R"([{"name": "V8_TQ_NAME"}])"),
      /*use_cpp_layouts=*/true, "V8_TQ_NAME needs an argument");
}

TEST(TorqueLayoutLoader, RejectsAnnotationOnTheWrongDeclaration) {
  // Tail annotations belong on the class, not on a field.
  ExpectFailingLayoutCompilation(
      TestScalarClass("uint32"),
      TestScalarJson(R"({"kind": "int", "width": 4, "signed": false})", 4,
                     R"([{"name": "V8_TQ_TAIL_NAME", "arg": "bytes"}])"),
      /*use_cpp_layouts=*/true, "V8_TQ_TAIL_NAME is not allowed here");
}

TEST(TorqueLayoutLoader, RejectsConflictingSynchronizationAnnotations) {
  ExpectFailingLayoutCompilation(
      TestScalarClass("uint32"),
      TestScalarJson(
          R"({"kind": "int", "width": 4, "signed": false})", 4,
          R"([{"name": "V8_TQ_RELAXED"}, {"name": "V8_TQ_ACQ_REL"}])"),
      /*use_cpp_layouts=*/true, "on the same member");
}

TEST(TorqueLayoutLoader, AppliesFieldNameAnnotation) {
  ExpectSuccessfulLayoutCompilation(
      TestScalarClass("renamed: uint32"),
      TestScalarJson(R"({"kind": "int", "width": 4, "signed": false})", 4,
                     R"([{"name": "V8_TQ_NAME", "arg": "renamed"}])"),
      /*use_cpp_layouts=*/true);
}

TEST(TorqueLayoutLoader, ImporterAppliesNamedExtent) {
  // The extent expression comes from V8_TQ_EXTENT_NAME through the
  // expression parser; the class size stays dynamic in Torque, so the
  // verifier checks that the array spans from the header to sizeof.
  ExpectSuccessfulLayoutCompilation(kTestLayoutNamedExtentClass,
                                    TestNamedExtentJson(),
                                    /*use_cpp_layouts=*/true);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
