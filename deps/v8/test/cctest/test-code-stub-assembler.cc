// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "src/api.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/builtins-promise-gen.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/compiler/node.h"
#include "src/debug/debug.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "test/cctest/compiler/code-assembler-tester.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {

using compiler::CodeAssemblerTester;
using compiler::FunctionTester;
using compiler::Node;
using compiler::CodeAssemblerLabel;
using compiler::CodeAssemblerVariable;
using compiler::CodeAssemblerVariableList;

namespace {

int sum9(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7,
         int a8) {
  return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8;
}

}  // namespace

TEST(CallCFunction9) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 0;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  {
    Node* const fun_constant = m.ExternalConstant(
        ExternalReference(reinterpret_cast<Address>(sum9), isolate));

    MachineType type_intptr = MachineType::IntPtr();

    Node* const result = m.CallCFunction9(
        type_intptr, type_intptr, type_intptr, type_intptr, type_intptr,
        type_intptr, type_intptr, type_intptr, type_intptr, type_intptr,
        fun_constant, m.IntPtrConstant(0), m.IntPtrConstant(1),
        m.IntPtrConstant(2), m.IntPtrConstant(3), m.IntPtrConstant(4),
        m.IntPtrConstant(5), m.IntPtrConstant(6), m.IntPtrConstant(7),
        m.IntPtrConstant(8));
    m.Return(m.SmiTag(result));
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<Object> result = ft.Call().ToHandleChecked();
  CHECK_EQ(36, Handle<Smi>::cast(result)->value());
}

namespace {

void CheckToUint32Result(uint32_t expected, Handle<Object> result) {
  const int64_t result_int64 = NumberToInt64(*result);
  const uint32_t result_uint32 = NumberToUint32(*result);

  CHECK_EQ(static_cast<int64_t>(result_uint32), result_int64);
  CHECK_EQ(expected, result_uint32);

  // Ensure that the result is normalized to a Smi, i.e. a HeapNumber is only
  // returned if the result is not within Smi range.
  const bool expected_fits_into_intptr =
      static_cast<int64_t>(expected) <=
      static_cast<int64_t>(std::numeric_limits<intptr_t>::max());
  if (expected_fits_into_intptr &&
      Smi::IsValid(static_cast<intptr_t>(expected))) {
    CHECK(result->IsSmi());
  } else {
    CHECK(result->IsHeapNumber());
  }
}

}  // namespace

TEST(ToUint32) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  Factory* factory = isolate->factory();

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  const int kContextOffset = 2;
  Node* const context = m.Parameter(kNumParams + kContextOffset);
  Node* const input = m.Parameter(0);
  m.Return(m.ToUint32(context, input));

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  // clang-format off
  double inputs[] = {
     std::nan("-1"), std::nan("1"), std::nan("2"),
    -std::numeric_limits<double>::infinity(),
     std::numeric_limits<double>::infinity(),
    -0.0, -0.001, -0.5, -0.999, -1.0,
     0.0,  0.001,  0.5,  0.999,  1.0,
    -2147483647.9, -2147483648.0, -2147483648.5, -2147483648.9,  // SmiMin.
     2147483646.9,  2147483647.0,  2147483647.5,  2147483647.9,  // SmiMax.
    -4294967295.9, -4294967296.0, -4294967296.5, -4294967297.0,  // - 2^32.
     4294967295.9,  4294967296.0,  4294967296.5,  4294967297.0,  //   2^32.
  };

  uint32_t expectations[] = {
     0, 0, 0,
     0,
     0,
     0, 0, 0, 0, 4294967295,
     0, 0, 0, 0, 1,
     2147483649, 2147483648, 2147483648, 2147483648,
     2147483646, 2147483647, 2147483647, 2147483647,
     1, 0, 0, 4294967295,
     4294967295, 0, 0, 1,
  };
  // clang-format on

  STATIC_ASSERT(arraysize(inputs) == arraysize(expectations));

  const int test_count = arraysize(inputs);
  for (int i = 0; i < test_count; i++) {
    Handle<Object> input_obj = factory->NewNumber(inputs[i]);
    Handle<HeapNumber> input_num;

    // Check with Smi input.
    if (input_obj->IsSmi()) {
      Handle<Smi> input_smi = Handle<Smi>::cast(input_obj);
      Handle<Object> result = ft.Call(input_smi).ToHandleChecked();
      CheckToUint32Result(expectations[i], result);
      input_num = factory->NewHeapNumber(inputs[i]);
    } else {
      input_num = Handle<HeapNumber>::cast(input_obj);
    }

    // Check with HeapNumber input.
    {
      CHECK(input_num->IsHeapNumber());
      Handle<Object> result = ft.Call(input_num).ToHandleChecked();
      CheckToUint32Result(expectations[i], result);
    }
  }

  // A couple of final cases for ToNumber conversions.
  CheckToUint32Result(0, ft.Call(factory->undefined_value()).ToHandleChecked());
  CheckToUint32Result(0, ft.Call(factory->null_value()).ToHandleChecked());
  CheckToUint32Result(0, ft.Call(factory->false_value()).ToHandleChecked());
  CheckToUint32Result(1, ft.Call(factory->true_value()).ToHandleChecked());
  CheckToUint32Result(
      42,
      ft.Call(factory->NewStringFromAsciiChecked("0x2A")).ToHandleChecked());

  ft.CheckThrows(factory->match_symbol());
}

TEST(FixedArrayAccessSmiIndex) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester data(isolate);
  CodeStubAssembler m(data.state());
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(5);
  array->set(4, Smi::FromInt(733));
  m.Return(m.LoadFixedArrayElement(m.HeapConstant(array),
                                   m.SmiTag(m.Int32Constant(4)), 0,
                                   CodeStubAssembler::SMI_PARAMETERS));
  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(733, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(LoadHeapNumberValue) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester data(isolate);
  CodeStubAssembler m(data.state());
  Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(1234);
  m.Return(m.SmiFromWord32(
      m.ChangeFloat64ToUint32(m.LoadHeapNumberValue(m.HeapConstant(number)))));
  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(1234, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(LoadInstanceType) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester data(isolate);
  CodeStubAssembler m(data.state());
  Handle<HeapObject> undefined = isolate->factory()->undefined_value();
  m.Return(m.SmiFromWord32(m.LoadInstanceType(m.HeapConstant(undefined))));
  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(InstanceType::ODDBALL_TYPE,
           Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(DecodeWordFromWord32) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester data(isolate);
  CodeStubAssembler m(data.state());

  class TestBitField : public BitField<unsigned, 3, 3> {};
  m.Return(
      m.SmiTag(m.DecodeWordFromWord32<TestBitField>(m.Int32Constant(0x2f))));
  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code);
  MaybeHandle<Object> result = ft.Call();
  // value  = 00101111
  // mask   = 00111000
  // result = 101
  CHECK_EQ(5, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(JSFunction) {
  const int kNumParams = 3;  // Receiver, left, right.
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());
  m.Return(m.SmiFromWord32(m.Int32Add(m.SmiToWord32(m.Parameter(1)),
                                      m.SmiToWord32(m.Parameter(2)))));

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  MaybeHandle<Object> result = ft.Call(isolate->factory()->undefined_value(),
                                       handle(Smi::FromInt(23), isolate),
                                       handle(Smi::FromInt(34), isolate));
  CHECK_EQ(57, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(ComputeIntegerHash) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 2;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());
  m.Return(m.SmiFromWord32(m.ComputeIntegerHash(
      m.SmiUntag(m.Parameter(0)), m.SmiToWord32(m.Parameter(1)))));

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<Smi> hash_seed = isolate->factory()->hash_seed();

  base::RandomNumberGenerator rand_gen(FLAG_random_seed);

  for (int i = 0; i < 1024; i++) {
    int k = rand_gen.NextInt(Smi::kMaxValue);

    Handle<Smi> key(Smi::FromInt(k), isolate);
    Handle<Object> result = ft.Call(key, hash_seed).ToHandleChecked();

    uint32_t hash = ComputeIntegerHash(k, hash_seed->value());
    Smi* expected = Smi::FromInt(hash & Smi::kMaxValue);
    CHECK_EQ(expected, Smi::cast(*result));
  }
}

TEST(ToString) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());
  m.Return(m.ToString(m.Parameter(kNumParams + 2), m.Parameter(0)));

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<FixedArray> test_cases = isolate->factory()->NewFixedArray(5);
  Handle<FixedArray> smi_test = isolate->factory()->NewFixedArray(2);
  smi_test->set(0, Smi::FromInt(42));
  Handle<String> str(isolate->factory()->InternalizeUtf8String("42"));
  smi_test->set(1, *str);
  test_cases->set(0, *smi_test);

  Handle<FixedArray> number_test = isolate->factory()->NewFixedArray(2);
  Handle<HeapNumber> num(isolate->factory()->NewHeapNumber(3.14));
  number_test->set(0, *num);
  str = isolate->factory()->InternalizeUtf8String("3.14");
  number_test->set(1, *str);
  test_cases->set(1, *number_test);

  Handle<FixedArray> string_test = isolate->factory()->NewFixedArray(2);
  str = isolate->factory()->InternalizeUtf8String("test");
  string_test->set(0, *str);
  string_test->set(1, *str);
  test_cases->set(2, *string_test);

  Handle<FixedArray> oddball_test = isolate->factory()->NewFixedArray(2);
  oddball_test->set(0, isolate->heap()->undefined_value());
  str = isolate->factory()->InternalizeUtf8String("undefined");
  oddball_test->set(1, *str);
  test_cases->set(3, *oddball_test);

  Handle<FixedArray> tostring_test = isolate->factory()->NewFixedArray(2);
  Handle<FixedArray> js_array_storage = isolate->factory()->NewFixedArray(2);
  js_array_storage->set(0, Smi::FromInt(1));
  js_array_storage->set(1, Smi::FromInt(2));
  Handle<JSArray> js_array = isolate->factory()->NewJSArray(2);
  JSArray::SetContent(js_array, js_array_storage);
  tostring_test->set(0, *js_array);
  str = isolate->factory()->InternalizeUtf8String("1,2");
  tostring_test->set(1, *str);
  test_cases->set(4, *tostring_test);

  for (int i = 0; i < 5; ++i) {
    Handle<FixedArray> test = handle(FixedArray::cast(test_cases->get(i)));
    Handle<Object> obj = handle(test->get(0), isolate);
    Handle<String> expected = handle(String::cast(test->get(1)));
    Handle<Object> result = ft.Call(obj).ToHandleChecked();
    CHECK(result->IsString());
    CHECK(String::Equals(Handle<String>::cast(result), expected));
  }
}

TEST(TryToName) {
  typedef CodeAssemblerLabel Label;
  typedef CodeAssemblerVariable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 3;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  enum Result { kKeyIsIndex, kKeyIsUnique, kBailout };
  {
    Node* key = m.Parameter(0);
    Node* expected_result = m.Parameter(1);
    Node* expected_arg = m.Parameter(2);

    Label passed(&m), failed(&m);
    Label if_keyisindex(&m), if_keyisunique(&m), if_bailout(&m);
    {
      Variable var_index(&m, MachineType::PointerRepresentation());
      Variable var_unique(&m, MachineRepresentation::kTagged);

      m.TryToName(key, &if_keyisindex, &var_index, &if_keyisunique, &var_unique,
                  &if_bailout);

      m.BIND(&if_keyisindex);
      m.GotoIfNot(m.WordEqual(expected_result,
                              m.SmiConstant(Smi::FromInt(kKeyIsIndex))),
                  &failed);
      m.Branch(m.WordEqual(m.SmiUntag(expected_arg), var_index.value()),
               &passed, &failed);

      m.BIND(&if_keyisunique);
      m.GotoIfNot(m.WordEqual(expected_result,
                              m.SmiConstant(Smi::FromInt(kKeyIsUnique))),
                  &failed);
      m.Branch(m.WordEqual(expected_arg, var_unique.value()), &passed, &failed);
    }

    m.BIND(&if_bailout);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kBailout))),
        &passed, &failed);

    m.BIND(&passed);
    m.Return(m.BooleanConstant(true));

    m.BIND(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<Object> expect_index(Smi::FromInt(kKeyIsIndex), isolate);
  Handle<Object> expect_unique(Smi::FromInt(kKeyIsUnique), isolate);
  Handle<Object> expect_bailout(Smi::FromInt(kBailout), isolate);

  {
    // TryToName(<zero smi>) => if_keyisindex: smi value.
    Handle<Object> key(Smi::kZero, isolate);
    ft.CheckTrue(key, expect_index, key);
  }

  {
    // TryToName(<positive smi>) => if_keyisindex: smi value.
    Handle<Object> key(Smi::FromInt(153), isolate);
    ft.CheckTrue(key, expect_index, key);
  }

  {
    // TryToName(<negative smi>) => if_keyisindex: smi value.
    // A subsequent bounds check needs to take care of this case.
    Handle<Object> key(Smi::FromInt(-1), isolate);
    ft.CheckTrue(key, expect_index, key);
  }

  {
    // TryToName(<heap number with int value>) => if_keyisindex: number.
    Handle<Object> key(isolate->factory()->NewHeapNumber(153));
    Handle<Object> index(Smi::FromInt(153), isolate);
    ft.CheckTrue(key, expect_index, index);
  }

  {
    // TryToName(<symbol>) => if_keyisunique: <symbol>.
    Handle<Object> key = isolate->factory()->NewSymbol();
    ft.CheckTrue(key, expect_unique, key);
  }

  {
    // TryToName(<internalized string>) => if_keyisunique: <internalized string>
    Handle<Object> key = isolate->factory()->InternalizeUtf8String("test");
    ft.CheckTrue(key, expect_unique, key);
  }

  {
    // TryToName(<internalized number string>) => if_keyisindex: number.
    Handle<Object> key = isolate->factory()->InternalizeUtf8String("153");
    Handle<Object> index(Smi::FromInt(153), isolate);
    ft.CheckTrue(key, expect_index, index);
  }

  {
    // TryToName(<internalized uncacheable number string>) => bailout
    Handle<Object> key =
        isolate->factory()->InternalizeUtf8String("4294967294");
    ft.CheckTrue(key, expect_bailout);
  }

  {
    // TryToName(<non-internalized number string>) => if_keyisindex: number.
    Handle<String> key = isolate->factory()->NewStringFromAsciiChecked("153");
    uint32_t dummy;
    CHECK(key->AsArrayIndex(&dummy));
    CHECK(key->HasHashCode());
    CHECK(!key->IsInternalizedString());
    Handle<Object> index(Smi::FromInt(153), isolate);
    ft.CheckTrue(key, expect_index, index);
  }

  {
    // TryToName(<number string without cached index>) => bailout.
    Handle<String> key = isolate->factory()->NewStringFromAsciiChecked("153");
    CHECK(!key->HasHashCode());
    ft.CheckTrue(key, expect_bailout);
  }

  {
    // TryToName(<non-internalized string>) => bailout.
    Handle<Object> key = isolate->factory()->NewStringFromAsciiChecked("test");
    ft.CheckTrue(key, expect_bailout);
  }

  if (FLAG_thin_strings) {
    // TryToName(<thin string>) => internalized version.
    Handle<String> s = isolate->factory()->NewStringFromAsciiChecked("foo");
    Handle<String> internalized = isolate->factory()->InternalizeString(s);
    ft.CheckTrue(s, expect_unique, internalized);
  }

  if (FLAG_thin_strings) {
    // TryToName(<thin two-byte string>) => internalized version.
    uc16 array1[] = {2001, 2002, 2003};
    Vector<const uc16> str1(array1);
    Handle<String> s =
        isolate->factory()->NewStringFromTwoByte(str1).ToHandleChecked();
    Handle<String> internalized = isolate->factory()->InternalizeString(s);
    ft.CheckTrue(s, expect_unique, internalized);
  }
}

namespace {

template <typename Dictionary>
void TestEntryToIndex() {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());
  {
    Node* entry = m.SmiUntag(m.Parameter(0));
    Node* result = m.EntryToIndex<Dictionary>(entry);
    m.Return(m.SmiTag(result));
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  // Test a wide range of entries but staying linear in the first 100 entries.
  for (int entry = 0; entry < Dictionary::kMaxCapacity;
       entry = entry * 1.01 + 1) {
    Handle<Object> result =
        ft.Call(handle(Smi::FromInt(entry), isolate)).ToHandleChecked();
    CHECK_EQ(Dictionary::EntryToIndex(entry), Smi::cast(*result)->value());
  }
}

TEST(NameDictionaryEntryToIndex) { TestEntryToIndex<NameDictionary>(); }
TEST(GlobalDictionaryEntryToIndex) { TestEntryToIndex<GlobalDictionary>(); }

}  // namespace

namespace {

template <typename Dictionary>
void TestNameDictionaryLookup() {
  typedef CodeAssemblerLabel Label;
  typedef CodeAssemblerVariable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  enum Result { kFound, kNotFound };
  {
    Node* dictionary = m.Parameter(0);
    Node* unique_name = m.Parameter(1);
    Node* expected_result = m.Parameter(2);
    Node* expected_arg = m.Parameter(3);

    Label passed(&m), failed(&m);
    Label if_found(&m), if_not_found(&m);
    Variable var_name_index(&m, MachineType::PointerRepresentation());

    m.NameDictionaryLookup<Dictionary>(dictionary, unique_name, &if_found,
                                       &var_name_index, &if_not_found);
    m.BIND(&if_found);
    m.GotoIfNot(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kFound))),
        &failed);
    m.Branch(m.WordEqual(m.SmiUntag(expected_arg), var_name_index.value()),
             &passed, &failed);

    m.BIND(&if_not_found);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kNotFound))),
        &passed, &failed);

    m.BIND(&passed);
    m.Return(m.BooleanConstant(true));

    m.BIND(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<Object> expect_found(Smi::FromInt(kFound), isolate);
  Handle<Object> expect_not_found(Smi::FromInt(kNotFound), isolate);

  Handle<Dictionary> dictionary = Dictionary::New(isolate, 40);
  PropertyDetails fake_details = PropertyDetails::Empty();

  Factory* factory = isolate->factory();
  Handle<Name> keys[] = {
      factory->InternalizeUtf8String("0"),
      factory->InternalizeUtf8String("42"),
      factory->InternalizeUtf8String("-153"),
      factory->InternalizeUtf8String("0.0"),
      factory->InternalizeUtf8String("4.2"),
      factory->InternalizeUtf8String(""),
      factory->InternalizeUtf8String("name"),
      factory->NewSymbol(),
      factory->NewPrivateSymbol(),
  };

  for (size_t i = 0; i < arraysize(keys); i++) {
    Handle<Object> value = factory->NewPropertyCell();
    dictionary = Dictionary::Add(dictionary, keys[i], value, fake_details);
  }

  for (size_t i = 0; i < arraysize(keys); i++) {
    int entry = dictionary->FindEntry(keys[i]);
    int name_index =
        Dictionary::EntryToIndex(entry) + Dictionary::kEntryKeyIndex;
    CHECK_NE(Dictionary::kNotFound, entry);

    Handle<Object> expected_name_index(Smi::FromInt(name_index), isolate);
    ft.CheckTrue(dictionary, keys[i], expect_found, expected_name_index);
  }

  Handle<Name> non_existing_keys[] = {
      factory->InternalizeUtf8String("1"),
      factory->InternalizeUtf8String("-42"),
      factory->InternalizeUtf8String("153"),
      factory->InternalizeUtf8String("-1.0"),
      factory->InternalizeUtf8String("1.3"),
      factory->InternalizeUtf8String("a"),
      factory->InternalizeUtf8String("boom"),
      factory->NewSymbol(),
      factory->NewPrivateSymbol(),
  };

  for (size_t i = 0; i < arraysize(non_existing_keys); i++) {
    int entry = dictionary->FindEntry(non_existing_keys[i]);
    CHECK_EQ(Dictionary::kNotFound, entry);

    ft.CheckTrue(dictionary, non_existing_keys[i], expect_not_found);
  }
}

}  // namespace

TEST(NameDictionaryLookup) { TestNameDictionaryLookup<NameDictionary>(); }

TEST(GlobalDictionaryLookup) { TestNameDictionaryLookup<GlobalDictionary>(); }

namespace {

template <typename Dictionary>
void TestNumberDictionaryLookup() {
  typedef CodeAssemblerLabel Label;
  typedef CodeAssemblerVariable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  enum Result { kFound, kNotFound };
  {
    Node* dictionary = m.Parameter(0);
    Node* key = m.SmiUntag(m.Parameter(1));
    Node* expected_result = m.Parameter(2);
    Node* expected_arg = m.Parameter(3);

    Label passed(&m), failed(&m);
    Label if_found(&m), if_not_found(&m);
    Variable var_entry(&m, MachineType::PointerRepresentation());

    m.NumberDictionaryLookup<Dictionary>(dictionary, key, &if_found, &var_entry,
                                         &if_not_found);
    m.BIND(&if_found);
    m.GotoIfNot(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kFound))),
        &failed);
    m.Branch(m.WordEqual(m.SmiUntag(expected_arg), var_entry.value()), &passed,
             &failed);

    m.BIND(&if_not_found);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kNotFound))),
        &passed, &failed);

    m.BIND(&passed);
    m.Return(m.BooleanConstant(true));

    m.BIND(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<Object> expect_found(Smi::FromInt(kFound), isolate);
  Handle<Object> expect_not_found(Smi::FromInt(kNotFound), isolate);

  const int kKeysCount = 1000;
  Handle<Dictionary> dictionary = Dictionary::New(isolate, kKeysCount);
  uint32_t keys[kKeysCount];

  Handle<Object> fake_value(Smi::FromInt(42), isolate);
  PropertyDetails fake_details = PropertyDetails::Empty();

  base::RandomNumberGenerator rand_gen(FLAG_random_seed);

  for (int i = 0; i < kKeysCount; i++) {
    int random_key = rand_gen.NextInt(Smi::kMaxValue);
    keys[i] = static_cast<uint32_t>(random_key);
    if (dictionary->FindEntry(keys[i]) != Dictionary::kNotFound) continue;

    dictionary = Dictionary::Add(dictionary, keys[i], fake_value, fake_details);
  }

  // Now try querying existing keys.
  for (int i = 0; i < kKeysCount; i++) {
    int entry = dictionary->FindEntry(keys[i]);
    CHECK_NE(Dictionary::kNotFound, entry);

    Handle<Object> key(Smi::FromInt(keys[i]), isolate);
    Handle<Object> expected_entry(Smi::FromInt(entry), isolate);
    ft.CheckTrue(dictionary, key, expect_found, expected_entry);
  }

  // Now try querying random keys which do not exist in the dictionary.
  for (int i = 0; i < kKeysCount;) {
    int random_key = rand_gen.NextInt(Smi::kMaxValue);
    int entry = dictionary->FindEntry(random_key);
    if (entry != Dictionary::kNotFound) continue;
    i++;

    Handle<Object> key(Smi::FromInt(random_key), isolate);
    ft.CheckTrue(dictionary, key, expect_not_found);
  }
}

}  // namespace

TEST(SeededNumberDictionaryLookup) {
  TestNumberDictionaryLookup<SeededNumberDictionary>();
}

TEST(UnseededNumberDictionaryLookup) {
  TestNumberDictionaryLookup<UnseededNumberDictionary>();
}

namespace {

void AddProperties(Handle<JSObject> object, Handle<Name> names[],
                   size_t count) {
  Isolate* isolate = object->GetIsolate();
  for (size_t i = 0; i < count; i++) {
    Handle<Object> value(Smi::FromInt(static_cast<int>(42 + i)), isolate);
    JSObject::AddProperty(object, names[i], value, NONE);
  }
}

Handle<AccessorPair> CreateAccessorPair(FunctionTester* ft,
                                        const char* getter_body,
                                        const char* setter_body) {
  Handle<AccessorPair> pair = ft->isolate->factory()->NewAccessorPair();
  if (getter_body) {
    pair->set_getter(*ft->NewFunction(getter_body));
  }
  if (setter_body) {
    pair->set_setter(*ft->NewFunction(setter_body));
  }
  return pair;
}

void AddProperties(Handle<JSObject> object, Handle<Name> names[],
                   size_t names_count, Handle<Object> values[],
                   size_t values_count, int seed = 0) {
  Isolate* isolate = object->GetIsolate();
  for (size_t i = 0; i < names_count; i++) {
    Handle<Object> value = values[(seed + i) % values_count];
    if (value->IsAccessorPair()) {
      Handle<AccessorPair> pair = Handle<AccessorPair>::cast(value);
      Handle<Object> getter(pair->getter(), isolate);
      Handle<Object> setter(pair->setter(), isolate);
      JSObject::DefineAccessor(object, names[i], getter, setter, NONE).Check();
    } else {
      JSObject::AddProperty(object, names[i], value, NONE);
    }
  }
}

}  // namespace

TEST(TryHasOwnProperty) {
  typedef CodeAssemblerLabel Label;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  enum Result { kFound, kNotFound, kBailout };
  {
    Node* object = m.Parameter(0);
    Node* unique_name = m.Parameter(1);
    Node* expected_result = m.Parameter(2);

    Label passed(&m), failed(&m);
    Label if_found(&m), if_not_found(&m), if_bailout(&m);

    Node* map = m.LoadMap(object);
    Node* instance_type = m.LoadMapInstanceType(map);

    m.TryHasOwnProperty(object, map, instance_type, unique_name, &if_found,
                        &if_not_found, &if_bailout);

    m.BIND(&if_found);
    m.Branch(m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kFound))),
             &passed, &failed);

    m.BIND(&if_not_found);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kNotFound))),
        &passed, &failed);

    m.BIND(&if_bailout);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kBailout))),
        &passed, &failed);

    m.BIND(&passed);
    m.Return(m.BooleanConstant(true));

    m.BIND(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<Object> expect_found(Smi::FromInt(kFound), isolate);
  Handle<Object> expect_not_found(Smi::FromInt(kNotFound), isolate);
  Handle<Object> expect_bailout(Smi::FromInt(kBailout), isolate);

  Factory* factory = isolate->factory();

  Handle<Name> deleted_property_name =
      factory->InternalizeUtf8String("deleted");

  Handle<Name> names[] = {
      factory->InternalizeUtf8String("a"),
      factory->InternalizeUtf8String("bb"),
      factory->InternalizeUtf8String("ccc"),
      factory->InternalizeUtf8String("dddd"),
      factory->InternalizeUtf8String("eeeee"),
      factory->InternalizeUtf8String(""),
      factory->InternalizeUtf8String("name"),
      factory->NewSymbol(),
      factory->NewPrivateSymbol(),
  };

  std::vector<Handle<JSObject>> objects;

  {
    // Fast object, no inobject properties.
    int inobject_properties = 0;
    Handle<Map> map = Map::Create(isolate, inobject_properties);
    Handle<JSObject> object = factory->NewJSObjectFromMap(map);
    AddProperties(object, names, arraysize(names));
    CHECK_EQ(JS_OBJECT_TYPE, object->map()->instance_type());
    CHECK_EQ(inobject_properties, object->map()->GetInObjectProperties());
    CHECK(!object->map()->is_dictionary_map());
    objects.push_back(object);
  }

  {
    // Fast object, all inobject properties.
    int inobject_properties = arraysize(names) * 2;
    Handle<Map> map = Map::Create(isolate, inobject_properties);
    Handle<JSObject> object = factory->NewJSObjectFromMap(map);
    AddProperties(object, names, arraysize(names));
    CHECK_EQ(JS_OBJECT_TYPE, object->map()->instance_type());
    CHECK_EQ(inobject_properties, object->map()->GetInObjectProperties());
    CHECK(!object->map()->is_dictionary_map());
    objects.push_back(object);
  }

  {
    // Fast object, half inobject properties.
    int inobject_properties = arraysize(names) / 2;
    Handle<Map> map = Map::Create(isolate, inobject_properties);
    Handle<JSObject> object = factory->NewJSObjectFromMap(map);
    AddProperties(object, names, arraysize(names));
    CHECK_EQ(JS_OBJECT_TYPE, object->map()->instance_type());
    CHECK_EQ(inobject_properties, object->map()->GetInObjectProperties());
    CHECK(!object->map()->is_dictionary_map());
    objects.push_back(object);
  }

  {
    // Dictionary mode object.
    Handle<JSFunction> function = factory->NewFunction(factory->empty_string());
    Handle<JSObject> object = factory->NewJSObject(function);
    AddProperties(object, names, arraysize(names));
    JSObject::NormalizeProperties(object, CLEAR_INOBJECT_PROPERTIES, 0, "test");

    JSObject::AddProperty(object, deleted_property_name, object, NONE);
    CHECK(JSObject::DeleteProperty(object, deleted_property_name, SLOPPY)
              .FromJust());

    CHECK_EQ(JS_OBJECT_TYPE, object->map()->instance_type());
    CHECK(object->map()->is_dictionary_map());
    objects.push_back(object);
  }

  {
    // Global object.
    Handle<JSFunction> function = factory->NewFunction(factory->empty_string());
    JSFunction::EnsureHasInitialMap(function);
    function->initial_map()->set_instance_type(JS_GLOBAL_OBJECT_TYPE);
    function->initial_map()->set_is_prototype_map(true);
    function->initial_map()->set_dictionary_map(true);
    Handle<JSObject> object = factory->NewJSGlobalObject(function);
    AddProperties(object, names, arraysize(names));

    JSObject::AddProperty(object, deleted_property_name, object, NONE);
    CHECK(JSObject::DeleteProperty(object, deleted_property_name, SLOPPY)
              .FromJust());

    CHECK_EQ(JS_GLOBAL_OBJECT_TYPE, object->map()->instance_type());
    CHECK(object->map()->is_dictionary_map());
    objects.push_back(object);
  }

  {
    for (Handle<JSObject> object : objects) {
      for (size_t name_index = 0; name_index < arraysize(names); name_index++) {
        Handle<Name> name = names[name_index];
        CHECK(JSReceiver::HasProperty(object, name).FromJust());
        ft.CheckTrue(object, name, expect_found);
      }
    }
  }

  {
    Handle<Name> non_existing_names[] = {
        factory->NewSymbol(),
        factory->InternalizeUtf8String("ne_a"),
        factory->InternalizeUtf8String("ne_bb"),
        factory->NewPrivateSymbol(),
        factory->InternalizeUtf8String("ne_ccc"),
        factory->InternalizeUtf8String("ne_dddd"),
        deleted_property_name,
    };
    for (Handle<JSObject> object : objects) {
      for (size_t key_index = 0; key_index < arraysize(non_existing_names);
           key_index++) {
        Handle<Name> name = non_existing_names[key_index];
        CHECK(!JSReceiver::HasProperty(object, name).FromJust());
        ft.CheckTrue(object, name, expect_not_found);
      }
    }
  }

  {
    Handle<JSFunction> function = factory->NewFunction(factory->empty_string());
    Handle<JSProxy> object = factory->NewJSProxy(function, objects[0]);
    CHECK_EQ(JS_PROXY_TYPE, object->map()->instance_type());
    ft.CheckTrue(object, names[0], expect_bailout);
  }

  {
    Handle<JSObject> object = isolate->global_proxy();
    CHECK_EQ(JS_GLOBAL_PROXY_TYPE, object->map()->instance_type());
    ft.CheckTrue(object, names[0], expect_bailout);
  }
}

TEST(TryGetOwnProperty) {
  typedef CodeAssemblerLabel Label;
  typedef CodeAssemblerVariable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());
  Factory* factory = isolate->factory();

  const int kNumParams = 2;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  Handle<Symbol> not_found_symbol = factory->NewSymbol();
  Handle<Symbol> bailout_symbol = factory->NewSymbol();
  {
    Node* object = m.Parameter(0);
    Node* unique_name = m.Parameter(1);
    Node* context = m.Parameter(kNumParams + 2);

    Variable var_value(&m, MachineRepresentation::kTagged);
    Label if_found(&m), if_not_found(&m), if_bailout(&m);

    Node* map = m.LoadMap(object);
    Node* instance_type = m.LoadMapInstanceType(map);

    m.TryGetOwnProperty(context, object, object, map, instance_type,
                        unique_name, &if_found, &var_value, &if_not_found,
                        &if_bailout);

    m.BIND(&if_found);
    m.Return(var_value.value());

    m.BIND(&if_not_found);
    m.Return(m.HeapConstant(not_found_symbol));

    m.BIND(&if_bailout);
    m.Return(m.HeapConstant(bailout_symbol));
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<Name> deleted_property_name =
      factory->InternalizeUtf8String("deleted");

  Handle<Name> names[] = {
      factory->InternalizeUtf8String("bb"),
      factory->NewSymbol(),
      factory->InternalizeUtf8String("a"),
      factory->InternalizeUtf8String("ccc"),
      factory->InternalizeUtf8String("esajefe"),
      factory->NewPrivateSymbol(),
      factory->InternalizeUtf8String("eeeee"),
      factory->InternalizeUtf8String("p1"),
      factory->InternalizeUtf8String("acshw23e"),
      factory->InternalizeUtf8String(""),
      factory->InternalizeUtf8String("dddd"),
      factory->NewPrivateSymbol(),
      factory->InternalizeUtf8String("name"),
      factory->InternalizeUtf8String("p2"),
      factory->InternalizeUtf8String("p3"),
      factory->InternalizeUtf8String("p4"),
      factory->NewPrivateSymbol(),
  };
  Handle<Object> values[] = {
      factory->NewFunction(factory->empty_string()),
      factory->NewSymbol(),
      factory->InternalizeUtf8String("a"),
      CreateAccessorPair(&ft, "() => 188;", "() => 199;"),
      factory->NewFunction(factory->InternalizeUtf8String("bb")),
      factory->InternalizeUtf8String("ccc"),
      CreateAccessorPair(&ft, "() => 88;", nullptr),
      handle(Smi::FromInt(1), isolate),
      factory->InternalizeUtf8String(""),
      CreateAccessorPair(&ft, nullptr, "() => 99;"),
      factory->NewHeapNumber(4.2),
      handle(Smi::FromInt(153), isolate),
      factory->NewJSObject(factory->NewFunction(factory->empty_string())),
      factory->NewPrivateSymbol(),
  };
  STATIC_ASSERT(arraysize(values) < arraysize(names));

  base::RandomNumberGenerator rand_gen(FLAG_random_seed);

  std::vector<Handle<JSObject>> objects;

  {
    // Fast object, no inobject properties.
    int inobject_properties = 0;
    Handle<Map> map = Map::Create(isolate, inobject_properties);
    Handle<JSObject> object = factory->NewJSObjectFromMap(map);
    AddProperties(object, names, arraysize(names), values, arraysize(values),
                  rand_gen.NextInt());
    CHECK_EQ(JS_OBJECT_TYPE, object->map()->instance_type());
    CHECK_EQ(inobject_properties, object->map()->GetInObjectProperties());
    CHECK(!object->map()->is_dictionary_map());
    objects.push_back(object);
  }

  {
    // Fast object, all inobject properties.
    int inobject_properties = arraysize(names) * 2;
    Handle<Map> map = Map::Create(isolate, inobject_properties);
    Handle<JSObject> object = factory->NewJSObjectFromMap(map);
    AddProperties(object, names, arraysize(names), values, arraysize(values),
                  rand_gen.NextInt());
    CHECK_EQ(JS_OBJECT_TYPE, object->map()->instance_type());
    CHECK_EQ(inobject_properties, object->map()->GetInObjectProperties());
    CHECK(!object->map()->is_dictionary_map());
    objects.push_back(object);
  }

  {
    // Fast object, half inobject properties.
    int inobject_properties = arraysize(names) / 2;
    Handle<Map> map = Map::Create(isolate, inobject_properties);
    Handle<JSObject> object = factory->NewJSObjectFromMap(map);
    AddProperties(object, names, arraysize(names), values, arraysize(values),
                  rand_gen.NextInt());
    CHECK_EQ(JS_OBJECT_TYPE, object->map()->instance_type());
    CHECK_EQ(inobject_properties, object->map()->GetInObjectProperties());
    CHECK(!object->map()->is_dictionary_map());
    objects.push_back(object);
  }

  {
    // Dictionary mode object.
    Handle<JSFunction> function = factory->NewFunction(factory->empty_string());
    Handle<JSObject> object = factory->NewJSObject(function);
    AddProperties(object, names, arraysize(names), values, arraysize(values),
                  rand_gen.NextInt());
    JSObject::NormalizeProperties(object, CLEAR_INOBJECT_PROPERTIES, 0, "test");

    JSObject::AddProperty(object, deleted_property_name, object, NONE);
    CHECK(JSObject::DeleteProperty(object, deleted_property_name, SLOPPY)
              .FromJust());

    CHECK_EQ(JS_OBJECT_TYPE, object->map()->instance_type());
    CHECK(object->map()->is_dictionary_map());
    objects.push_back(object);
  }

  {
    // Global object.
    Handle<JSGlobalObject> object = isolate->global_object();
    AddProperties(object, names, arraysize(names), values, arraysize(values),
                  rand_gen.NextInt());

    JSObject::AddProperty(object, deleted_property_name, object, NONE);
    CHECK(JSObject::DeleteProperty(object, deleted_property_name, SLOPPY)
              .FromJust());

    CHECK_EQ(JS_GLOBAL_OBJECT_TYPE, object->map()->instance_type());
    CHECK(object->map()->is_dictionary_map());
    objects.push_back(object);
  }

  // TODO(ishell): test proxy and interceptors when they are supported.

  {
    for (Handle<JSObject> object : objects) {
      for (size_t name_index = 0; name_index < arraysize(names); name_index++) {
        Handle<Name> name = names[name_index];
        Handle<Object> expected_value =
            JSReceiver::GetProperty(object, name).ToHandleChecked();
        Handle<Object> value = ft.Call(object, name).ToHandleChecked();
        CHECK(expected_value->SameValue(*value));
      }
    }
  }

  {
    Handle<Name> non_existing_names[] = {
        factory->NewSymbol(),
        factory->InternalizeUtf8String("ne_a"),
        factory->InternalizeUtf8String("ne_bb"),
        factory->NewPrivateSymbol(),
        factory->InternalizeUtf8String("ne_ccc"),
        factory->InternalizeUtf8String("ne_dddd"),
        deleted_property_name,
    };
    for (Handle<JSObject> object : objects) {
      for (size_t key_index = 0; key_index < arraysize(non_existing_names);
           key_index++) {
        Handle<Name> name = non_existing_names[key_index];
        Handle<Object> expected_value =
            JSReceiver::GetProperty(object, name).ToHandleChecked();
        CHECK(expected_value->IsUndefined(isolate));
        Handle<Object> value = ft.Call(object, name).ToHandleChecked();
        CHECK_EQ(*not_found_symbol, *value);
      }
    }
  }

  {
    Handle<JSFunction> function = factory->NewFunction(factory->empty_string());
    Handle<JSProxy> object = factory->NewJSProxy(function, objects[0]);
    CHECK_EQ(JS_PROXY_TYPE, object->map()->instance_type());
    Handle<Object> value = ft.Call(object, names[0]).ToHandleChecked();
    // Proxies are not supported yet.
    CHECK_EQ(*bailout_symbol, *value);
  }

  {
    Handle<JSObject> object = isolate->global_proxy();
    CHECK_EQ(JS_GLOBAL_PROXY_TYPE, object->map()->instance_type());
    // Global proxies are not supported yet.
    Handle<Object> value = ft.Call(object, names[0]).ToHandleChecked();
    CHECK_EQ(*bailout_symbol, *value);
  }
}

namespace {

void AddElement(Handle<JSObject> object, uint32_t index, Handle<Object> value,
                PropertyAttributes attributes = NONE) {
  JSObject::AddDataElement(object, index, value, attributes).ToHandleChecked();
}

}  // namespace

TEST(TryLookupElement) {
  typedef CodeAssemblerLabel Label;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 3;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  enum Result { kFound, kAbsent, kNotFound, kBailout };
  {
    Node* object = m.Parameter(0);
    Node* index = m.SmiUntag(m.Parameter(1));
    Node* expected_result = m.Parameter(2);

    Label passed(&m), failed(&m);
    Label if_found(&m), if_not_found(&m), if_bailout(&m), if_absent(&m);

    Node* map = m.LoadMap(object);
    Node* instance_type = m.LoadMapInstanceType(map);

    m.TryLookupElement(object, map, instance_type, index, &if_found, &if_absent,
                       &if_not_found, &if_bailout);

    m.BIND(&if_found);
    m.Branch(m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kFound))),
             &passed, &failed);

    m.BIND(&if_absent);
    m.Branch(m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kAbsent))),
             &passed, &failed);

    m.BIND(&if_not_found);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kNotFound))),
        &passed, &failed);

    m.BIND(&if_bailout);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kBailout))),
        &passed, &failed);

    m.BIND(&passed);
    m.Return(m.BooleanConstant(true));

    m.BIND(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Factory* factory = isolate->factory();
  Handle<Object> smi0(Smi::kZero, isolate);
  Handle<Object> smi1(Smi::FromInt(1), isolate);
  Handle<Object> smi7(Smi::FromInt(7), isolate);
  Handle<Object> smi13(Smi::FromInt(13), isolate);
  Handle<Object> smi42(Smi::FromInt(42), isolate);

  Handle<Object> expect_found(Smi::FromInt(kFound), isolate);
  Handle<Object> expect_absent(Smi::FromInt(kAbsent), isolate);
  Handle<Object> expect_not_found(Smi::FromInt(kNotFound), isolate);
  Handle<Object> expect_bailout(Smi::FromInt(kBailout), isolate);

#define CHECK_FOUND(object, index)                         \
  CHECK(JSReceiver::HasElement(object, index).FromJust()); \
  ft.CheckTrue(object, smi##index, expect_found);

#define CHECK_NOT_FOUND(object, index)                      \
  CHECK(!JSReceiver::HasElement(object, index).FromJust()); \
  ft.CheckTrue(object, smi##index, expect_not_found);

#define CHECK_ABSENT(object, index)                                        \
  {                                                                        \
    bool success;                                                          \
    Handle<Smi> smi(Smi::FromInt(index), isolate);                         \
    LookupIterator it =                                                    \
        LookupIterator::PropertyOrElement(isolate, object, smi, &success); \
    CHECK(success);                                                        \
    CHECK(!JSReceiver::HasProperty(&it).FromJust());                       \
    ft.CheckTrue(object, smi, expect_absent);                              \
  }

  {
    Handle<JSArray> object = factory->NewJSArray(0, FAST_SMI_ELEMENTS);
    AddElement(object, 0, smi0);
    AddElement(object, 1, smi0);
    CHECK_EQ(FAST_SMI_ELEMENTS, object->map()->elements_kind());

    CHECK_FOUND(object, 0);
    CHECK_FOUND(object, 1);
    CHECK_NOT_FOUND(object, 7);
    CHECK_NOT_FOUND(object, 13);
    CHECK_NOT_FOUND(object, 42);
  }

  {
    Handle<JSArray> object = factory->NewJSArray(0, FAST_HOLEY_SMI_ELEMENTS);
    AddElement(object, 0, smi0);
    AddElement(object, 13, smi0);
    CHECK_EQ(FAST_HOLEY_SMI_ELEMENTS, object->map()->elements_kind());

    CHECK_FOUND(object, 0);
    CHECK_NOT_FOUND(object, 1);
    CHECK_NOT_FOUND(object, 7);
    CHECK_FOUND(object, 13);
    CHECK_NOT_FOUND(object, 42);
  }

  {
    Handle<JSArray> object = factory->NewJSArray(0, FAST_ELEMENTS);
    AddElement(object, 0, smi0);
    AddElement(object, 1, smi0);
    CHECK_EQ(FAST_ELEMENTS, object->map()->elements_kind());

    CHECK_FOUND(object, 0);
    CHECK_FOUND(object, 1);
    CHECK_NOT_FOUND(object, 7);
    CHECK_NOT_FOUND(object, 13);
    CHECK_NOT_FOUND(object, 42);
  }

  {
    Handle<JSArray> object = factory->NewJSArray(0, FAST_HOLEY_ELEMENTS);
    AddElement(object, 0, smi0);
    AddElement(object, 13, smi0);
    CHECK_EQ(FAST_HOLEY_ELEMENTS, object->map()->elements_kind());

    CHECK_FOUND(object, 0);
    CHECK_NOT_FOUND(object, 1);
    CHECK_NOT_FOUND(object, 7);
    CHECK_FOUND(object, 13);
    CHECK_NOT_FOUND(object, 42);
  }

  {
    Handle<JSTypedArray> object = factory->NewJSTypedArray(INT32_ELEMENTS, 2);
    Local<v8::ArrayBuffer> buffer = Utils::ToLocal(object->GetBuffer());

    CHECK_EQ(INT32_ELEMENTS, object->map()->elements_kind());

    CHECK_FOUND(object, 0);
    CHECK_FOUND(object, 1);
    CHECK_ABSENT(object, -10);
    CHECK_ABSENT(object, 13);
    CHECK_ABSENT(object, 42);

    v8::ArrayBuffer::Contents contents = buffer->Externalize();
    buffer->Neuter();
    isolate->array_buffer_allocator()->Free(contents.Data(),
                                            contents.ByteLength());

    CHECK_ABSENT(object, 0);
    CHECK_ABSENT(object, 1);
    CHECK_ABSENT(object, -10);
    CHECK_ABSENT(object, 13);
    CHECK_ABSENT(object, 42);
  }

  {
    Handle<JSFunction> constructor = isolate->string_function();
    Handle<JSObject> object = factory->NewJSObject(constructor);
    Handle<String> str = factory->InternalizeUtf8String("ab");
    Handle<JSValue>::cast(object)->set_value(*str);
    AddElement(object, 13, smi0);
    CHECK_EQ(FAST_STRING_WRAPPER_ELEMENTS, object->map()->elements_kind());

    CHECK_FOUND(object, 0);
    CHECK_FOUND(object, 1);
    CHECK_NOT_FOUND(object, 7);
    CHECK_FOUND(object, 13);
    CHECK_NOT_FOUND(object, 42);
  }

  {
    Handle<JSFunction> constructor = isolate->string_function();
    Handle<JSObject> object = factory->NewJSObject(constructor);
    Handle<String> str = factory->InternalizeUtf8String("ab");
    Handle<JSValue>::cast(object)->set_value(*str);
    AddElement(object, 13, smi0);
    JSObject::NormalizeElements(object);
    CHECK_EQ(SLOW_STRING_WRAPPER_ELEMENTS, object->map()->elements_kind());

    CHECK_FOUND(object, 0);
    CHECK_FOUND(object, 1);
    CHECK_NOT_FOUND(object, 7);
    CHECK_FOUND(object, 13);
    CHECK_NOT_FOUND(object, 42);
  }

// TODO(ishell): uncomment once NO_ELEMENTS kind is supported.
//  {
//    Handle<Map> map = Map::Create(isolate, 0);
//    map->set_elements_kind(NO_ELEMENTS);
//    Handle<JSObject> object = factory->NewJSObjectFromMap(map);
//    CHECK_EQ(NO_ELEMENTS, object->map()->elements_kind());
//
//    CHECK_NOT_FOUND(object, 0);
//    CHECK_NOT_FOUND(object, 1);
//    CHECK_NOT_FOUND(object, 7);
//    CHECK_NOT_FOUND(object, 13);
//    CHECK_NOT_FOUND(object, 42);
//  }

#undef CHECK_FOUND
#undef CHECK_NOT_FOUND

  {
    Handle<JSArray> handler = factory->NewJSArray(0);
    Handle<JSFunction> function = factory->NewFunction(factory->empty_string());
    Handle<JSProxy> object = factory->NewJSProxy(function, handler);
    CHECK_EQ(JS_PROXY_TYPE, object->map()->instance_type());
    ft.CheckTrue(object, smi0, expect_bailout);
  }

  {
    Handle<JSObject> object = isolate->global_object();
    CHECK_EQ(JS_GLOBAL_OBJECT_TYPE, object->map()->instance_type());
    ft.CheckTrue(object, smi0, expect_bailout);
  }

  {
    Handle<JSObject> object = isolate->global_proxy();
    CHECK_EQ(JS_GLOBAL_PROXY_TYPE, object->map()->instance_type());
    ft.CheckTrue(object, smi0, expect_bailout);
  }
}

TEST(AllocateJSObjectFromMap) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  Factory* factory = isolate->factory();

  const int kNumParams = 3;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  {
    Node* map = m.Parameter(0);
    Node* properties = m.Parameter(1);
    Node* elements = m.Parameter(2);

    Node* result = m.AllocateJSObjectFromMap(map, properties, elements);

    m.Return(result);
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<Map> maps[] = {
      handle(isolate->object_function()->initial_map(), isolate),
      handle(isolate->array_function()->initial_map(), isolate),
  };

#define VERIFY(result, map_value, properties_value, elements_value) \
  CHECK_EQ(result->map(), map_value);                               \
  CHECK_EQ(result->properties(), properties_value);                 \
  CHECK_EQ(result->elements(), elements_value);

  {
    Handle<Object> empty_fixed_array = factory->empty_fixed_array();
    for (size_t i = 0; i < arraysize(maps); i++) {
      Handle<Map> map = maps[i];
      Handle<JSObject> result = Handle<JSObject>::cast(
          ft.Call(map, empty_fixed_array, empty_fixed_array).ToHandleChecked());
      VERIFY(result, *map, *empty_fixed_array, *empty_fixed_array);
      CHECK(result->HasFastProperties());
#ifdef VERIFY_HEAP
      isolate->heap()->Verify();
#endif
    }
  }

  {
    // TODO(cbruni): handle in-object properties
    Handle<JSObject> object = Handle<JSObject>::cast(
        v8::Utils::OpenHandle(*CompileRun("var object = {a:1,b:2, 1:1, 2:2}; "
                                          "object")));
    JSObject::NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, 0,
                                  "Normalize");
    Handle<JSObject> result = Handle<JSObject>::cast(
        ft.Call(handle(object->map()), handle(object->properties()),
                handle(object->elements()))
            .ToHandleChecked());
    VERIFY(result, object->map(), object->properties(), object->elements());
    CHECK(!result->HasFastProperties());
#ifdef VERIFY_HEAP
    isolate->heap()->Verify();
#endif
  }
#undef VERIFY
}

TEST(AllocateNameDictionary) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  {
    Node* capacity = m.Parameter(0);
    Node* result = m.AllocateNameDictionary(m.SmiUntag(capacity));
    m.Return(result);
  }

  Handle<Code> code = data.GenerateCode();
  FunctionTester ft(code, kNumParams);

  {
    for (int i = 0; i < 256; i = i * 1.1 + 1) {
      Handle<Object> result =
          ft.Call(handle(Smi::FromInt(i), isolate)).ToHandleChecked();
      Handle<NameDictionary> dict = NameDictionary::New(isolate, i);
      // Both dictionaries should be memory equal.
      int size =
          FixedArrayBase::kHeaderSize + (dict->length() - 1) * kPointerSize;
      CHECK_EQ(0, memcmp(*dict, *result, size));
    }
  }
}

TEST(PopAndReturnConstant) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;
  const int kNumProgrammaticParams = 2;
  CodeAssemblerTester data(isolate, kNumParams - kNumProgrammaticParams);
  CodeStubAssembler m(data.state());

  // Call a function that return |kNumProgramaticParams| parameters in addition
  // to those specified by the static descriptor. |kNumProgramaticParams| is
  // specified as a constant.
  m.PopAndReturn(m.Int32Constant(kNumProgrammaticParams),
                 m.SmiConstant(Smi::FromInt(1234)));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result;
  for (int test_count = 0; test_count < 100; ++test_count) {
    result = ft.Call(isolate->factory()->undefined_value(),
                     Handle<Smi>(Smi::FromInt(1234), isolate),
                     isolate->factory()->undefined_value(),
                     isolate->factory()->undefined_value())
                 .ToHandleChecked();
    CHECK_EQ(1234, Handle<Smi>::cast(result)->value());
  }
}

TEST(PopAndReturnVariable) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;
  const int kNumProgrammaticParams = 2;
  CodeAssemblerTester data(isolate, kNumParams - kNumProgrammaticParams);
  CodeStubAssembler m(data.state());

  // Call a function that return |kNumProgramaticParams| parameters in addition
  // to those specified by the static descriptor. |kNumProgramaticParams| is
  // passed in as a parameter to the function so that it can't be recongized as
  // a constant.
  m.PopAndReturn(m.SmiUntag(m.Parameter(1)), m.SmiConstant(Smi::FromInt(1234)));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result;
  for (int test_count = 0; test_count < 100; ++test_count) {
    result = ft.Call(isolate->factory()->undefined_value(),
                     Handle<Smi>(Smi::FromInt(1234), isolate),
                     isolate->factory()->undefined_value(),
                     Handle<Smi>(Smi::FromInt(kNumProgrammaticParams), isolate))
                 .ToHandleChecked();
    CHECK_EQ(1234, Handle<Smi>::cast(result)->value());
  }
}

TEST(OneToTwoByteStringCopy) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 2;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  m.CopyStringCharacters(
      m.Parameter(0), m.Parameter(1), m.SmiConstant(Smi::FromInt(0)),
      m.SmiConstant(Smi::FromInt(0)), m.SmiConstant(Smi::FromInt(5)),
      String::ONE_BYTE_ENCODING, String::TWO_BYTE_ENCODING,
      CodeStubAssembler::SMI_PARAMETERS);
  m.Return(m.SmiConstant(Smi::FromInt(0)));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  Handle<String> string1 = isolate->factory()->InternalizeUtf8String("abcde");
  uc16 array[] = {1000, 1001, 1002, 1003, 1004};
  Vector<const uc16> str(array);
  Handle<String> string2 =
      isolate->factory()->NewStringFromTwoByte(str).ToHandleChecked();
  FunctionTester ft(code, 2);
  ft.Call(string1, string2);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[0],
           Handle<SeqTwoByteString>::cast(string2)->GetChars()[0]);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[1],
           Handle<SeqTwoByteString>::cast(string2)->GetChars()[1]);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[2],
           Handle<SeqTwoByteString>::cast(string2)->GetChars()[2]);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[3],
           Handle<SeqTwoByteString>::cast(string2)->GetChars()[3]);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[4],
           Handle<SeqTwoByteString>::cast(string2)->GetChars()[4]);
}

TEST(OneToOneByteStringCopy) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 2;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  m.CopyStringCharacters(
      m.Parameter(0), m.Parameter(1), m.SmiConstant(Smi::FromInt(0)),
      m.SmiConstant(Smi::FromInt(0)), m.SmiConstant(Smi::FromInt(5)),
      String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING,
      CodeStubAssembler::SMI_PARAMETERS);
  m.Return(m.SmiConstant(Smi::FromInt(0)));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  Handle<String> string1 = isolate->factory()->InternalizeUtf8String("abcde");
  uint8_t array[] = {100, 101, 102, 103, 104};
  Vector<const uint8_t> str(array);
  Handle<String> string2 =
      isolate->factory()->NewStringFromOneByte(str).ToHandleChecked();
  FunctionTester ft(code, 2);
  ft.Call(string1, string2);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[0],
           Handle<SeqOneByteString>::cast(string2)->GetChars()[0]);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[1],
           Handle<SeqOneByteString>::cast(string2)->GetChars()[1]);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[2],
           Handle<SeqOneByteString>::cast(string2)->GetChars()[2]);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[3],
           Handle<SeqOneByteString>::cast(string2)->GetChars()[3]);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[4],
           Handle<SeqOneByteString>::cast(string2)->GetChars()[4]);
}

TEST(OneToOneByteStringCopyNonZeroStart) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 2;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  m.CopyStringCharacters(
      m.Parameter(0), m.Parameter(1), m.SmiConstant(Smi::FromInt(0)),
      m.SmiConstant(Smi::FromInt(3)), m.SmiConstant(Smi::FromInt(2)),
      String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING,
      CodeStubAssembler::SMI_PARAMETERS);
  m.Return(m.SmiConstant(Smi::FromInt(0)));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  Handle<String> string1 = isolate->factory()->InternalizeUtf8String("abcde");
  uint8_t array[] = {100, 101, 102, 103, 104};
  Vector<const uint8_t> str(array);
  Handle<String> string2 =
      isolate->factory()->NewStringFromOneByte(str).ToHandleChecked();
  FunctionTester ft(code, 2);
  ft.Call(string1, string2);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[0],
           Handle<SeqOneByteString>::cast(string2)->GetChars()[3]);
  CHECK_EQ(Handle<SeqOneByteString>::cast(string1)->GetChars()[1],
           Handle<SeqOneByteString>::cast(string2)->GetChars()[4]);
  CHECK_EQ(100, Handle<SeqOneByteString>::cast(string2)->GetChars()[0]);
  CHECK_EQ(101, Handle<SeqOneByteString>::cast(string2)->GetChars()[1]);
  CHECK_EQ(102, Handle<SeqOneByteString>::cast(string2)->GetChars()[2]);
}

TEST(TwoToTwoByteStringCopy) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 2;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  m.CopyStringCharacters(
      m.Parameter(0), m.Parameter(1), m.SmiConstant(Smi::FromInt(0)),
      m.SmiConstant(Smi::FromInt(0)), m.SmiConstant(Smi::FromInt(5)),
      String::TWO_BYTE_ENCODING, String::TWO_BYTE_ENCODING,
      CodeStubAssembler::SMI_PARAMETERS);
  m.Return(m.SmiConstant(Smi::FromInt(0)));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  uc16 array1[] = {2000, 2001, 2002, 2003, 2004};
  Vector<const uc16> str1(array1);
  Handle<String> string1 =
      isolate->factory()->NewStringFromTwoByte(str1).ToHandleChecked();
  uc16 array2[] = {1000, 1001, 1002, 1003, 1004};
  Vector<const uc16> str2(array2);
  Handle<String> string2 =
      isolate->factory()->NewStringFromTwoByte(str2).ToHandleChecked();
  FunctionTester ft(code, 2);
  ft.Call(string1, string2);
  CHECK_EQ(Handle<SeqTwoByteString>::cast(string1)->GetChars()[0],
           Handle<SeqTwoByteString>::cast(string2)->GetChars()[0]);
  CHECK_EQ(Handle<SeqTwoByteString>::cast(string1)->GetChars()[1],
           Handle<SeqTwoByteString>::cast(string2)->GetChars()[1]);
  CHECK_EQ(Handle<SeqTwoByteString>::cast(string1)->GetChars()[2],
           Handle<SeqTwoByteString>::cast(string2)->GetChars()[2]);
  CHECK_EQ(Handle<SeqTwoByteString>::cast(string1)->GetChars()[3],
           Handle<SeqTwoByteString>::cast(string2)->GetChars()[3]);
  CHECK_EQ(Handle<SeqTwoByteString>::cast(string1)->GetChars()[4],
           Handle<SeqTwoByteString>::cast(string2)->GetChars()[4]);
}

TEST(Arguments) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  CodeStubArguments arguments(&m, m.IntPtrConstant(3));

  CSA_ASSERT(
      &m, m.WordEqual(arguments.AtIndex(0), m.SmiConstant(Smi::FromInt(12))));
  CSA_ASSERT(
      &m, m.WordEqual(arguments.AtIndex(1), m.SmiConstant(Smi::FromInt(13))));
  CSA_ASSERT(
      &m, m.WordEqual(arguments.AtIndex(2), m.SmiConstant(Smi::FromInt(14))));

  m.Return(arguments.GetReceiver());

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result = ft.Call(isolate->factory()->undefined_value(),
                                  Handle<Smi>(Smi::FromInt(12), isolate),
                                  Handle<Smi>(Smi::FromInt(13), isolate),
                                  Handle<Smi>(Smi::FromInt(14), isolate))
                              .ToHandleChecked();
  CHECK_EQ(*isolate->factory()->undefined_value(), *result);
}

TEST(ArgumentsForEach) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  CodeStubArguments arguments(&m, m.IntPtrConstant(3));

  CodeAssemblerVariable sum(&m, MachineRepresentation::kTagged);
  CodeAssemblerVariableList list({&sum}, m.zone());

  sum.Bind(m.SmiConstant(0));

  arguments.ForEach(
      list, [&m, &sum](Node* arg) { sum.Bind(m.SmiAdd(sum.value(), arg)); });

  m.Return(sum.value());

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result = ft.Call(isolate->factory()->undefined_value(),
                                  Handle<Smi>(Smi::FromInt(12), isolate),
                                  Handle<Smi>(Smi::FromInt(13), isolate),
                                  Handle<Smi>(Smi::FromInt(14), isolate))
                              .ToHandleChecked();
  CHECK_EQ(Smi::FromInt(12 + 13 + 14), *result);
}

TEST(IsDebugActive) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  CodeAssemblerLabel if_active(&m), if_not_active(&m);

  m.Branch(m.IsDebugActive(), &if_active, &if_not_active);
  m.BIND(&if_active);
  m.Return(m.TrueConstant());
  m.BIND(&if_not_active);
  m.Return(m.FalseConstant());

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  CHECK(!isolate->debug()->is_active());
  Handle<Object> result =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->false_value(), *result);

  bool* debug_is_active = reinterpret_cast<bool*>(
      ExternalReference::debug_is_active_address(isolate).address());

  // Cheat to enable debug (TODO: do this properly).
  *debug_is_active = true;

  result = ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->true_value(), *result);

  // Reset debug mode.
  *debug_is_active = false;
}

class AppendJSArrayCodeStubAssembler : public CodeStubAssembler {
 public:
  AppendJSArrayCodeStubAssembler(compiler::CodeAssemblerState* state,
                                 ElementsKind kind)
      : CodeStubAssembler(state), kind_(kind) {}

  void TestAppendJSArrayImpl(Isolate* isolate, CodeAssemblerTester* tester,
                             Object* o1, Object* o2, Object* o3, Object* o4,
                             int initial_size, int result_size) {
    typedef CodeAssemblerVariable Variable;
    typedef CodeAssemblerLabel Label;
    Handle<JSArray> array = isolate->factory()->NewJSArray(
        kind_, 2, initial_size, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
    JSObject::SetElement(isolate, array, 0,
                         Handle<Smi>(Smi::FromInt(1), isolate), SLOPPY)
        .Check();
    JSObject::SetElement(isolate, array, 1,
                         Handle<Smi>(Smi::FromInt(2), isolate), SLOPPY)
        .Check();
    CodeStubArguments args(this, IntPtrConstant(kNumParams));
    Variable arg_index(this, MachineType::PointerRepresentation());
    Label bailout(this);
    arg_index.Bind(IntPtrConstant(0));
    Node* length = BuildAppendJSArray(kind_, HeapConstant(array), args,
                                      arg_index, &bailout);
    Return(length);

    BIND(&bailout);
    Return(SmiTag(IntPtrAdd(arg_index.value(), IntPtrConstant(2))));

    Handle<Code> code = tester->GenerateCode();
    CHECK(!code.is_null());

    FunctionTester ft(code, kNumParams);

    Handle<Object> result =
        ft.Call(Handle<Object>(o1, isolate), Handle<Object>(o2, isolate),
                Handle<Object>(o3, isolate), Handle<Object>(o4, isolate))
            .ToHandleChecked();

    CHECK_EQ(kind_, array->GetElementsKind());
    CHECK_EQ(result_size, Handle<Smi>::cast(result)->value());
    CHECK_EQ(result_size, Smi::cast(array->length())->value());
    Object* obj = *JSObject::GetElement(isolate, array, 2).ToHandleChecked();
    CHECK_EQ(result_size < 3 ? isolate->heap()->undefined_value() : o1, obj);
    obj = *JSObject::GetElement(isolate, array, 3).ToHandleChecked();
    CHECK_EQ(result_size < 4 ? isolate->heap()->undefined_value() : o2, obj);
    obj = *JSObject::GetElement(isolate, array, 4).ToHandleChecked();
    CHECK_EQ(result_size < 5 ? isolate->heap()->undefined_value() : o3, obj);
    obj = *JSObject::GetElement(isolate, array, 5).ToHandleChecked();
    CHECK_EQ(result_size < 6 ? isolate->heap()->undefined_value() : o4, obj);
  }

  static void TestAppendJSArray(Isolate* isolate, ElementsKind kind, Object* o1,
                                Object* o2, Object* o3, Object* o4,
                                int initial_size, int result_size) {
    CodeAssemblerTester data(isolate, kNumParams);
    AppendJSArrayCodeStubAssembler m(data.state(), kind);
    m.TestAppendJSArrayImpl(isolate, &data, o1, o2, o3, o4, initial_size,
                            result_size);
  }

 private:
  static const int kNumParams = 4;
  ElementsKind kind_;
};

TEST(BuildAppendJSArrayFastElement) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  AppendJSArrayCodeStubAssembler::TestAppendJSArray(
      isolate, FAST_ELEMENTS, Smi::FromInt(3), Smi::FromInt(4), Smi::FromInt(5),
      Smi::FromInt(6), 6, 6);
}

TEST(BuildAppendJSArrayFastElementGrow) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  AppendJSArrayCodeStubAssembler::TestAppendJSArray(
      isolate, FAST_ELEMENTS, Smi::FromInt(3), Smi::FromInt(4), Smi::FromInt(5),
      Smi::FromInt(6), 2, 6);
}

TEST(BuildAppendJSArrayFastSmiElement) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  AppendJSArrayCodeStubAssembler::TestAppendJSArray(
      isolate, FAST_SMI_ELEMENTS, Smi::FromInt(3), Smi::FromInt(4),
      Smi::FromInt(5), Smi::FromInt(6), 6, 6);
}

TEST(BuildAppendJSArrayFastSmiElementGrow) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  AppendJSArrayCodeStubAssembler::TestAppendJSArray(
      isolate, FAST_SMI_ELEMENTS, Smi::FromInt(3), Smi::FromInt(4),
      Smi::FromInt(5), Smi::FromInt(6), 2, 6);
}

TEST(BuildAppendJSArrayFastSmiElementObject) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  AppendJSArrayCodeStubAssembler::TestAppendJSArray(
      isolate, FAST_SMI_ELEMENTS, Smi::FromInt(3), Smi::FromInt(4),
      isolate->heap()->undefined_value(), Smi::FromInt(6), 6, 4);
}

TEST(BuildAppendJSArrayFastSmiElementObjectGrow) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  AppendJSArrayCodeStubAssembler::TestAppendJSArray(
      isolate, FAST_SMI_ELEMENTS, Smi::FromInt(3), Smi::FromInt(4),
      isolate->heap()->undefined_value(), Smi::FromInt(6), 2, 4);
}

TEST(BuildAppendJSArrayFastDoubleElements) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  AppendJSArrayCodeStubAssembler::TestAppendJSArray(
      isolate, FAST_DOUBLE_ELEMENTS, Smi::FromInt(3), Smi::FromInt(4),
      Smi::FromInt(5), Smi::FromInt(6), 6, 6);
}

TEST(BuildAppendJSArrayFastDoubleElementsGrow) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  AppendJSArrayCodeStubAssembler::TestAppendJSArray(
      isolate, FAST_DOUBLE_ELEMENTS, Smi::FromInt(3), Smi::FromInt(4),
      Smi::FromInt(5), Smi::FromInt(6), 2, 6);
}

TEST(BuildAppendJSArrayFastDoubleElementsObject) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  AppendJSArrayCodeStubAssembler::TestAppendJSArray(
      isolate, FAST_DOUBLE_ELEMENTS, Smi::FromInt(3), Smi::FromInt(4),
      isolate->heap()->undefined_value(), Smi::FromInt(6), 6, 4);
}

namespace {

template <typename Stub, typename... Args>
void Recompile(Args... args) {
  Stub stub(args...);
  stub.DeleteStubFromCacheForTesting();
  stub.GetCode();
}

}  // namespace

void CustomPromiseHook(v8::PromiseHookType type, v8::Local<v8::Promise> promise,
                       v8::Local<v8::Value> parentPromise) {}

TEST(IsPromiseHookEnabled) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  m.Return(m.SelectBooleanConstant(m.IsPromiseHookEnabledOrDebugIsActive()));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->false_value(), *result);

  isolate->SetPromiseHook(CustomPromiseHook);
  result = ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->true_value(), *result);

  isolate->SetPromiseHook(nullptr);
  result = ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->false_value(), *result);
}

TEST(AllocateAndInitJSPromise) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  PromiseBuiltinsAssembler m(data.state());

  Node* const context = m.Parameter(kNumParams + 2);
  Node* const promise = m.AllocateAndInitJSPromise(context);
  m.Return(promise);

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK(result->IsJSPromise());
}

TEST(AllocateAndSetJSPromise) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  PromiseBuiltinsAssembler m(data.state());

  Node* const context = m.Parameter(kNumParams + 2);
  Node* const promise = m.AllocateAndSetJSPromise(
      context, m.SmiConstant(v8::Promise::kPending), m.SmiConstant(1));
  m.Return(promise);

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK(result->IsJSPromise());
  Handle<JSPromise> js_promise = Handle<JSPromise>::cast(result);
  CHECK_EQ(v8::Promise::kPending, js_promise->status());
  CHECK_EQ(Smi::FromInt(1), js_promise->result());
  CHECK(!js_promise->has_handler());
}

TEST(AllocatePromiseReactionJobInfo) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());
  PromiseBuiltinsAssembler p(data.state());

  Node* const context = m.Parameter(kNumParams + 2);
  Node* const tasks = m.AllocateFixedArray(FAST_ELEMENTS, m.IntPtrConstant(1));
  m.StoreFixedArrayElement(tasks, 0, m.UndefinedConstant());
  Node* const deferred_promise =
      m.AllocateFixedArray(FAST_ELEMENTS, m.IntPtrConstant(1));
  m.StoreFixedArrayElement(deferred_promise, 0, m.UndefinedConstant());
  Node* const info = m.AllocatePromiseReactionJobInfo(
      m.SmiConstant(1), tasks, deferred_promise, m.UndefinedConstant(),
      m.UndefinedConstant(), context);
  m.Return(info);

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK(result->IsPromiseReactionJobInfo());
  Handle<PromiseReactionJobInfo> promise_info =
      Handle<PromiseReactionJobInfo>::cast(result);
  CHECK_EQ(Smi::FromInt(1), promise_info->value());
  CHECK(promise_info->tasks()->IsFixedArray());
  CHECK(promise_info->deferred_promise()->IsFixedArray());
  CHECK(promise_info->deferred_on_resolve()->IsUndefined(isolate));
  CHECK(promise_info->deferred_on_reject()->IsUndefined(isolate));
  CHECK(promise_info->context()->IsContext());
}

TEST(AllocatePromiseResolveThenableJobInfo) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  PromiseBuiltinsAssembler p(data.state());

  Node* const context = p.Parameter(kNumParams + 2);
  Node* const native_context = p.LoadNativeContext(context);
  Node* const thenable = p.AllocateAndInitJSPromise(context);
  Node* const then =
      p.GetProperty(context, thenable, isolate->factory()->then_string());
  Node* resolve = nullptr;
  Node* reject = nullptr;
  std::tie(resolve, reject) = p.CreatePromiseResolvingFunctions(
      thenable, p.FalseConstant(), native_context);

  Node* const info = p.AllocatePromiseResolveThenableJobInfo(
      thenable, then, resolve, reject, context);
  p.Return(info);

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK(result->IsPromiseResolveThenableJobInfo());
  Handle<PromiseResolveThenableJobInfo> promise_info =
      Handle<PromiseResolveThenableJobInfo>::cast(result);
  CHECK(promise_info->thenable()->IsJSPromise());
  CHECK(promise_info->then()->IsJSFunction());
  CHECK(promise_info->resolve()->IsJSFunction());
  CHECK(promise_info->reject()->IsJSFunction());
  CHECK(promise_info->context()->IsContext());
}

TEST(IsSymbol) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  Node* const symbol = m.Parameter(0);
  m.Return(m.SelectBooleanConstant(m.IsSymbol(symbol)));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result =
      ft.Call(isolate->factory()->NewSymbol()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->true_value(), *result);

  result = ft.Call(isolate->factory()->empty_string()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->false_value(), *result);
}

TEST(IsPrivateSymbol) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());

  Node* const symbol = m.Parameter(0);
  m.Return(m.SelectBooleanConstant(m.IsPrivateSymbol(symbol)));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result =
      ft.Call(isolate->factory()->NewSymbol()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->false_value(), *result);

  result = ft.Call(isolate->factory()->empty_string()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->false_value(), *result);

  result = ft.Call(isolate->factory()->NewPrivateSymbol()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->true_value(), *result);
}

TEST(PromiseHasHandler) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  PromiseBuiltinsAssembler m(data.state());

  Node* const context = m.Parameter(kNumParams + 2);
  Node* const promise =
      m.AllocateAndInitJSPromise(context, m.UndefinedConstant());
  m.Return(m.SelectBooleanConstant(m.PromiseHasHandler(promise)));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK_EQ(isolate->heap()->false_value(), *result);
}

TEST(CreatePromiseResolvingFunctionsContext) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  PromiseBuiltinsAssembler m(data.state());

  Node* const context = m.Parameter(kNumParams + 2);
  Node* const native_context = m.LoadNativeContext(context);
  Node* const promise =
      m.AllocateAndInitJSPromise(context, m.UndefinedConstant());
  Node* const promise_context = m.CreatePromiseResolvingFunctionsContext(
      promise, m.BooleanConstant(false), native_context);
  m.Return(promise_context);

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK(result->IsContext());
  Handle<Context> context_js = Handle<Context>::cast(result);
  CHECK_EQ(isolate->native_context()->closure(), context_js->closure());
  CHECK_EQ(isolate->heap()->the_hole_value(), context_js->extension());
  CHECK_EQ(*isolate->native_context(), context_js->native_context());
  CHECK_EQ(Smi::FromInt(0),
           context_js->get(PromiseBuiltinsAssembler::kAlreadyVisitedSlot));
  CHECK(context_js->get(PromiseBuiltinsAssembler::kPromiseSlot)->IsJSPromise());
  CHECK_EQ(isolate->heap()->false_value(),
           context_js->get(PromiseBuiltinsAssembler::kDebugEventSlot));
}

TEST(CreatePromiseResolvingFunctions) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  PromiseBuiltinsAssembler m(data.state());

  Node* const context = m.Parameter(kNumParams + 2);
  Node* const native_context = m.LoadNativeContext(context);
  Node* const promise =
      m.AllocateAndInitJSPromise(context, m.UndefinedConstant());
  Node *resolve, *reject;
  std::tie(resolve, reject) = m.CreatePromiseResolvingFunctions(
      promise, m.BooleanConstant(false), native_context);
  Node* const kSize = m.IntPtrConstant(2);
  Node* const arr = m.AllocateFixedArray(FAST_ELEMENTS, kSize);
  m.StoreFixedArrayElement(arr, 0, resolve);
  m.StoreFixedArrayElement(arr, 1, reject);
  m.Return(arr);

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result_obj =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK(result_obj->IsFixedArray());
  Handle<FixedArray> result_arr = Handle<FixedArray>::cast(result_obj);
  CHECK(result_arr->get(0)->IsJSFunction());
  CHECK(result_arr->get(1)->IsJSFunction());
}

TEST(NewElementsCapacity) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester data(isolate, 1);
  CodeStubAssembler m(data.state());
  m.Return(m.SmiTag(m.CalculateNewElementsCapacity(
      m.SmiUntag(m.Parameter(0)), CodeStubAssembler::INTPTR_PARAMETERS)));
  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());
  FunctionTester ft(code, 1);
  Handle<Smi> test_value = Handle<Smi>(Smi::FromInt(0), isolate);
  Handle<Smi> result_obj =
      Handle<Smi>::cast(ft.Call(test_value).ToHandleChecked());
  CHECK_EQ(
      result_obj->value(),
      static_cast<int>(JSObject::NewElementsCapacity(test_value->value())));
  test_value = Handle<Smi>(Smi::FromInt(1), isolate);
  result_obj = Handle<Smi>::cast(ft.Call(test_value).ToHandleChecked());
  CHECK_EQ(
      result_obj->value(),
      static_cast<int>(JSObject::NewElementsCapacity(test_value->value())));
  test_value = Handle<Smi>(Smi::FromInt(2), isolate);
  result_obj = Handle<Smi>::cast(ft.Call(test_value).ToHandleChecked());
  CHECK_EQ(
      result_obj->value(),
      static_cast<int>(JSObject::NewElementsCapacity(test_value->value())));
  test_value = Handle<Smi>(Smi::FromInt(1025), isolate);
  result_obj = Handle<Smi>::cast(ft.Call(test_value).ToHandleChecked());
  CHECK_EQ(
      result_obj->value(),
      static_cast<int>(JSObject::NewElementsCapacity(test_value->value())));
}

TEST(NewElementsCapacitySmi) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeAssemblerTester data(isolate, 1);
  CodeStubAssembler m(data.state());
  m.Return(m.CalculateNewElementsCapacity(m.Parameter(0),
                                          CodeStubAssembler::SMI_PARAMETERS));
  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());
  FunctionTester ft(code, 1);
  Handle<Smi> test_value = Handle<Smi>(Smi::FromInt(0), isolate);
  Handle<Smi> result_obj =
      Handle<Smi>::cast(ft.Call(test_value).ToHandleChecked());
  CHECK_EQ(
      result_obj->value(),
      static_cast<int>(JSObject::NewElementsCapacity(test_value->value())));
  test_value = Handle<Smi>(Smi::FromInt(1), isolate);
  result_obj = Handle<Smi>::cast(ft.Call(test_value).ToHandleChecked());
  CHECK_EQ(
      result_obj->value(),
      static_cast<int>(JSObject::NewElementsCapacity(test_value->value())));
  test_value = Handle<Smi>(Smi::FromInt(2), isolate);
  result_obj = Handle<Smi>::cast(ft.Call(test_value).ToHandleChecked());
  CHECK_EQ(
      result_obj->value(),
      static_cast<int>(JSObject::NewElementsCapacity(test_value->value())));
  test_value = Handle<Smi>(Smi::FromInt(1025), isolate);
  result_obj = Handle<Smi>::cast(ft.Call(test_value).ToHandleChecked());
  CHECK_EQ(
      result_obj->value(),
      static_cast<int>(JSObject::NewElementsCapacity(test_value->value())));
}

TEST(AllocateFunctionWithMapAndContext) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  PromiseBuiltinsAssembler m(data.state());

  Node* const context = m.Parameter(kNumParams + 2);
  Node* const native_context = m.LoadNativeContext(context);
  Node* const promise =
      m.AllocateAndInitJSPromise(context, m.UndefinedConstant());
  Node* promise_context = m.CreatePromiseResolvingFunctionsContext(
      promise, m.BooleanConstant(false), native_context);
  Node* resolve_info =
      m.LoadContextElement(native_context, Context::PROMISE_RESOLVE_SHARED_FUN);
  Node* const map = m.LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
  Node* const resolve =
      m.AllocateFunctionWithMapAndContext(map, resolve_info, promise_context);
  m.Return(resolve);

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result_obj =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK(result_obj->IsJSFunction());
  Handle<JSFunction> fun = Handle<JSFunction>::cast(result_obj);
  CHECK_EQ(isolate->heap()->empty_fixed_array(), fun->properties());
  CHECK_EQ(isolate->heap()->empty_fixed_array(), fun->elements());
  CHECK_EQ(isolate->heap()->undefined_cell(), fun->feedback_vector_cell());
  CHECK_EQ(isolate->heap()->the_hole_value(), fun->prototype_or_initial_map());
  CHECK_EQ(*isolate->promise_resolve_shared_fun(), fun->shared());
  CHECK_EQ(isolate->promise_resolve_shared_fun()->code(), fun->code());
  CHECK_EQ(isolate->heap()->undefined_value(), fun->next_function_link());
}

TEST(CreatePromiseGetCapabilitiesExecutorContext) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeAssemblerTester data(isolate, kNumParams);
  PromiseBuiltinsAssembler m(data.state());

  Node* const context = m.Parameter(kNumParams + 2);
  Node* const native_context = m.LoadNativeContext(context);

  Node* const map = m.LoadRoot(Heap::kJSPromiseCapabilityMapRootIndex);
  Node* const capability = m.AllocateJSObjectFromMap(map);
  m.StoreObjectFieldNoWriteBarrier(
      capability, JSPromiseCapability::kPromiseOffset, m.UndefinedConstant());
  m.StoreObjectFieldNoWriteBarrier(
      capability, JSPromiseCapability::kResolveOffset, m.UndefinedConstant());
  m.StoreObjectFieldNoWriteBarrier(
      capability, JSPromiseCapability::kRejectOffset, m.UndefinedConstant());
  Node* const executor_context =
      m.CreatePromiseGetCapabilitiesExecutorContext(capability, native_context);
  m.Return(executor_context);

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result_obj =
      ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
  CHECK(result_obj->IsContext());
  Handle<Context> context_js = Handle<Context>::cast(result_obj);
  CHECK_EQ(PromiseBuiltinsAssembler::kCapabilitiesContextLength,
           context_js->length());
  CHECK_EQ(isolate->native_context()->closure(), context_js->closure());
  CHECK_EQ(isolate->heap()->the_hole_value(), context_js->extension());
  CHECK_EQ(*isolate->native_context(), context_js->native_context());
  CHECK(context_js->get(PromiseBuiltinsAssembler::kCapabilitySlot)
            ->IsJSPromiseCapability());
}

TEST(NewPromiseCapability) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  {  // Builtin Promise
    const int kNumParams = 1;
    CodeAssemblerTester data(isolate, kNumParams);
    PromiseBuiltinsAssembler m(data.state());

    Node* const context = m.Parameter(kNumParams + 2);
    Node* const native_context = m.LoadNativeContext(context);
    Node* const promise_constructor =
        m.LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);

    Node* const capability =
        m.NewPromiseCapability(context, promise_constructor);
    m.Return(capability);

    Handle<Code> code = data.GenerateCode();
    FunctionTester ft(code, kNumParams);

    Handle<Object> result_obj =
        ft.Call(isolate->factory()->undefined_value()).ToHandleChecked();
    CHECK(result_obj->IsJSPromiseCapability());
    Handle<JSPromiseCapability> result =
        Handle<JSPromiseCapability>::cast(result_obj);

    CHECK(result->promise()->IsJSPromise());
    CHECK(result->resolve()->IsJSFunction());
    CHECK(result->reject()->IsJSFunction());
    CHECK_EQ(isolate->native_context()->promise_resolve_shared_fun(),
             JSFunction::cast(result->resolve())->shared());
    CHECK_EQ(isolate->native_context()->promise_reject_shared_fun(),
             JSFunction::cast(result->reject())->shared());

    Handle<JSFunction> callbacks[] = {
        handle(JSFunction::cast(result->resolve())),
        handle(JSFunction::cast(result->reject()))};

    for (auto&& callback : callbacks) {
      Handle<Context> context(Context::cast(callback->context()));
      CHECK_EQ(isolate->native_context()->closure(), context->closure());
      CHECK_EQ(isolate->heap()->the_hole_value(), context->extension());
      CHECK_EQ(*isolate->native_context(), context->native_context());
      CHECK_EQ(PromiseBuiltinsAssembler::kPromiseContextLength,
               context->length());
      CHECK_EQ(context->get(PromiseBuiltinsAssembler::kPromiseSlot),
               result->promise());
    }
  }

  {  // Custom Promise
    const int kNumParams = 2;
    CodeAssemblerTester data(isolate, kNumParams);
    PromiseBuiltinsAssembler m(data.state());

    Node* const context = m.Parameter(kNumParams + 2);

    Node* const constructor = m.Parameter(1);
    Node* const capability = m.NewPromiseCapability(context, constructor);
    m.Return(capability);

    Handle<Code> code = data.GenerateCode();
    FunctionTester ft(code, kNumParams);

    Handle<JSFunction> constructor_fn =
        Handle<JSFunction>::cast(v8::Utils::OpenHandle(*CompileRun(
            "(function FakePromise(executor) {"
            "  var self = this;"
            "  function resolve(value) { self.resolvedValue = value; }"
            "  function reject(reason) { self.rejectedReason = reason; }"
            "  executor(resolve, reject);"
            "})")));

    Handle<Object> result_obj =
        ft.Call(isolate->factory()->undefined_value(), constructor_fn)
            .ToHandleChecked();
    CHECK(result_obj->IsJSPromiseCapability());
    Handle<JSPromiseCapability> result =
        Handle<JSPromiseCapability>::cast(result_obj);

    CHECK(result->promise()->IsJSObject());
    Handle<JSObject> promise(JSObject::cast(result->promise()));
    CHECK_EQ(constructor_fn->prototype_or_initial_map(), promise->map());
    CHECK(result->resolve()->IsJSFunction());
    CHECK(result->reject()->IsJSFunction());

    Handle<String> resolved_str =
        isolate->factory()->NewStringFromAsciiChecked("resolvedStr");
    Handle<String> rejected_str =
        isolate->factory()->NewStringFromAsciiChecked("rejectedStr");

    Handle<Object> argv1[] = {resolved_str};
    Handle<Object> ret =
        Execution::Call(isolate, handle(result->resolve(), isolate),
                        isolate->factory()->undefined_value(), 1, argv1)
            .ToHandleChecked();

    Handle<Object> prop1 =
        JSReceiver::GetProperty(isolate, promise, "resolvedValue")
            .ToHandleChecked();
    CHECK_EQ(*resolved_str, *prop1);

    Handle<Object> argv2[] = {rejected_str};
    ret = Execution::Call(isolate, handle(result->reject(), isolate),
                          isolate->factory()->undefined_value(), 1, argv2)
              .ToHandleChecked();
    Handle<Object> prop2 =
        JSReceiver::GetProperty(isolate, promise, "rejectedReason")
            .ToHandleChecked();
    CHECK_EQ(*rejected_str, *prop2);
  }
}

TEST(DirectMemoryTest8BitWord32Immediate) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  typedef CodeAssemblerLabel Label;

  const int kNumParams = 0;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());
  int8_t buffer[] = {1, 2, 4, 8, 17, 33, 65, 127};
  const int element_count = 8;
  Label bad(&m);

  Node* buffer_node = m.IntPtrConstant(reinterpret_cast<intptr_t>(buffer));
  for (size_t i = 0; i < element_count; ++i) {
    for (size_t j = 0; j < element_count; ++j) {
      Node* loaded = m.LoadBufferObject(buffer_node, static_cast<int>(i),
                                        MachineType::Uint8());
      Node* masked = m.Word32And(loaded, m.Int32Constant(buffer[j]));
      if ((buffer[j] & buffer[i]) != 0) {
        m.GotoIf(m.Word32Equal(masked, m.Int32Constant(0)), &bad);
      } else {
        m.GotoIf(m.Word32NotEqual(masked, m.Int32Constant(0)), &bad);
      }
    }
  }

  m.Return(m.SmiConstant(1));

  m.BIND(&bad);
  m.Return(m.SmiConstant(0));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());
  FunctionTester ft(code, kNumParams);
  CHECK_EQ(1, Handle<Smi>::cast(ft.Call().ToHandleChecked())->value());
}

TEST(DirectMemoryTest16BitWord32Immediate) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  typedef CodeAssemblerLabel Label;

  const int kNumParams = 0;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());
  int16_t buffer[] = {156, 2234, 4544, 8444, 1723, 3888, 658, 1278};
  const int element_count = 8;
  Label bad(&m);

  Node* buffer_node = m.IntPtrConstant(reinterpret_cast<intptr_t>(buffer));
  for (size_t i = 0; i < element_count; ++i) {
    for (size_t j = 0; j < element_count; ++j) {
      Node* loaded =
          m.LoadBufferObject(buffer_node, static_cast<int>(i * sizeof(int16_t)),
                             MachineType::Uint16());
      Node* masked = m.Word32And(loaded, m.Int32Constant(buffer[j]));
      if ((buffer[j] & buffer[i]) != 0) {
        m.GotoIf(m.Word32Equal(masked, m.Int32Constant(0)), &bad);
      } else {
        m.GotoIf(m.Word32NotEqual(masked, m.Int32Constant(0)), &bad);
      }
    }
  }

  m.Return(m.SmiConstant(1));

  m.BIND(&bad);
  m.Return(m.SmiConstant(0));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());
  FunctionTester ft(code, kNumParams);
  CHECK_EQ(1, Handle<Smi>::cast(ft.Call().ToHandleChecked())->value());
}

TEST(DirectMemoryTest8BitWord32) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  typedef CodeAssemblerLabel Label;

  const int kNumParams = 0;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());
  int8_t buffer[] = {1, 2, 4, 8, 17, 33, 65, 127, 67, 38};
  const int element_count = 10;
  Label bad(&m);
  Node* constants[element_count];

  Node* buffer_node = m.IntPtrConstant(reinterpret_cast<intptr_t>(buffer));
  for (size_t i = 0; i < element_count; ++i) {
    constants[i] = m.LoadBufferObject(buffer_node, static_cast<int>(i),
                                      MachineType::Uint8());
  }

  for (size_t i = 0; i < element_count; ++i) {
    for (size_t j = 0; j < element_count; ++j) {
      Node* loaded = m.LoadBufferObject(buffer_node, static_cast<int>(i),
                                        MachineType::Uint8());
      Node* masked = m.Word32And(loaded, constants[j]);
      if ((buffer[j] & buffer[i]) != 0) {
        m.GotoIf(m.Word32Equal(masked, m.Int32Constant(0)), &bad);
      } else {
        m.GotoIf(m.Word32NotEqual(masked, m.Int32Constant(0)), &bad);
      }

      masked = m.Word32And(constants[i], constants[j]);
      if ((buffer[j] & buffer[i]) != 0) {
        m.GotoIf(m.Word32Equal(masked, m.Int32Constant(0)), &bad);
      } else {
        m.GotoIf(m.Word32NotEqual(masked, m.Int32Constant(0)), &bad);
      }
    }
  }

  m.Return(m.SmiConstant(1));

  m.BIND(&bad);
  m.Return(m.SmiConstant(0));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());
  FunctionTester ft(code, kNumParams);
  CHECK_EQ(1, Handle<Smi>::cast(ft.Call().ToHandleChecked())->value());
}

TEST(DirectMemoryTest16BitWord32) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  typedef CodeAssemblerLabel Label;

  const int kNumParams = 0;
  CodeAssemblerTester data(isolate, kNumParams);
  CodeStubAssembler m(data.state());
  int16_t buffer[] = {1, 2, 4, 8, 12345, 33, 65, 255, 67, 3823};
  const int element_count = 10;
  Label bad(&m);
  Node* constants[element_count];

  Node* buffer_node1 = m.IntPtrConstant(reinterpret_cast<intptr_t>(buffer));
  for (size_t i = 0; i < element_count; ++i) {
    constants[i] =
        m.LoadBufferObject(buffer_node1, static_cast<int>(i * sizeof(int16_t)),
                           MachineType::Uint16());
  }
  Node* buffer_node2 = m.IntPtrConstant(reinterpret_cast<intptr_t>(buffer));

  for (size_t i = 0; i < element_count; ++i) {
    for (size_t j = 0; j < element_count; ++j) {
      Node* loaded = m.LoadBufferObject(buffer_node1,
                                        static_cast<int>(i * sizeof(int16_t)),
                                        MachineType::Uint16());
      Node* masked = m.Word32And(loaded, constants[j]);
      if ((buffer[j] & buffer[i]) != 0) {
        m.GotoIf(m.Word32Equal(masked, m.Int32Constant(0)), &bad);
      } else {
        m.GotoIf(m.Word32NotEqual(masked, m.Int32Constant(0)), &bad);
      }

      // Force a memory access relative to a high-number register.
      loaded = m.LoadBufferObject(buffer_node2,
                                  static_cast<int>(i * sizeof(int16_t)),
                                  MachineType::Uint16());
      masked = m.Word32And(loaded, constants[j]);
      if ((buffer[j] & buffer[i]) != 0) {
        m.GotoIf(m.Word32Equal(masked, m.Int32Constant(0)), &bad);
      } else {
        m.GotoIf(m.Word32NotEqual(masked, m.Int32Constant(0)), &bad);
      }

      masked = m.Word32And(constants[i], constants[j]);
      if ((buffer[j] & buffer[i]) != 0) {
        m.GotoIf(m.Word32Equal(masked, m.Int32Constant(0)), &bad);
      } else {
        m.GotoIf(m.Word32NotEqual(masked, m.Int32Constant(0)), &bad);
      }
    }
  }

  m.Return(m.SmiConstant(1));

  m.BIND(&bad);
  m.Return(m.SmiConstant(0));

  Handle<Code> code = data.GenerateCode();
  CHECK(!code.is_null());
  FunctionTester ft(code, kNumParams);
  CHECK_EQ(1, Handle<Smi>::cast(ft.Call().ToHandleChecked())->value());
}

}  // namespace internal
}  // namespace v8
