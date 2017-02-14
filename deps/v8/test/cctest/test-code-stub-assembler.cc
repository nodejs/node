// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/utils/random-number-generator.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/compiler/node.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "test/cctest/compiler/code-assembler-tester.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {

using compiler::FunctionTester;
using compiler::Node;

typedef compiler::CodeAssemblerTesterImpl<CodeStubAssembler>
    CodeStubAssemblerTester;

TEST(FixedArrayAccessSmiIndex) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(5);
  array->set(4, Smi::FromInt(733));
  m.Return(m.LoadFixedArrayElement(m.HeapConstant(array),
                                   m.SmiTag(m.Int32Constant(4)), 0,
                                   CodeStubAssembler::SMI_PARAMETERS));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(733, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(LoadHeapNumberValue) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(1234);
  m.Return(m.SmiTag(
      m.ChangeFloat64ToUint32(m.LoadHeapNumberValue(m.HeapConstant(number)))));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(1234, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(LoadInstanceType) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Handle<HeapObject> undefined = isolate->factory()->undefined_value();
  m.Return(m.SmiTag(m.LoadInstanceType(m.HeapConstant(undefined))));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  CHECK_EQ(InstanceType::ODDBALL_TYPE,
           Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(DecodeWordFromWord32) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);

  class TestBitField : public BitField<unsigned, 3, 3> {};
  m.Return(
      m.SmiTag(m.DecodeWordFromWord32<TestBitField>(m.Int32Constant(0x2f))));
  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(descriptor, code);
  MaybeHandle<Object> result = ft.Call();
  // value  = 00101111
  // mask   = 00111000
  // result = 101
  CHECK_EQ(5, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(JSFunction) {
  const int kNumParams = 3;  // Receiver, left, right.
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeStubAssemblerTester m(isolate, kNumParams);
  m.Return(m.SmiFromWord32(m.Int32Add(m.SmiToWord32(m.Parameter(1)),
                                      m.SmiToWord32(m.Parameter(2)))));

  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(code, kNumParams);

  MaybeHandle<Object> result = ft.Call(isolate->factory()->undefined_value(),
                                       handle(Smi::FromInt(23), isolate),
                                       handle(Smi::FromInt(34), isolate));
  CHECK_EQ(57, Handle<Smi>::cast(result.ToHandleChecked())->value());
}

TEST(ComputeIntegerHash) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 2;
  CodeStubAssemblerTester m(isolate, kNumParams);
  m.Return(m.SmiFromWord32(m.ComputeIntegerHash(
      m.SmiToWord32(m.Parameter(0)), m.SmiToWord32(m.Parameter(1)))));

  Handle<Code> code = m.GenerateCode();
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
  CodeStubAssemblerTester m(isolate, kNumParams);
  m.Return(m.ToString(m.Parameter(kNumParams + 2), m.Parameter(0)));

  Handle<Code> code = m.GenerateCode();
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

TEST(FlattenString) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 1;
  CodeStubAssemblerTester m(isolate, kNumParams);
  m.Return(m.FlattenString(m.Parameter(0)));

  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Handle<FixedArray> test_cases(isolate->factory()->NewFixedArray(4));
  Handle<String> expected(
      isolate->factory()->InternalizeUtf8String("hello, world!"));
  test_cases->set(0, *expected);

  Handle<String> string(
      isolate->factory()->InternalizeUtf8String("filler hello, world! filler"));
  Handle<String> sub_string(
      isolate->factory()->NewProperSubString(string, 7, 20));
  test_cases->set(1, *sub_string);

  Handle<String> hello(isolate->factory()->InternalizeUtf8String("hello,"));
  Handle<String> world(isolate->factory()->InternalizeUtf8String(" world!"));
  Handle<String> cons_str(
      isolate->factory()->NewConsString(hello, world).ToHandleChecked());
  test_cases->set(2, *cons_str);

  Handle<String> empty(isolate->factory()->InternalizeUtf8String(""));
  Handle<String> fake_cons_str(
      isolate->factory()->NewConsString(expected, empty).ToHandleChecked());
  test_cases->set(3, *fake_cons_str);

  for (int i = 0; i < 4; ++i) {
    Handle<String> test = handle(String::cast(test_cases->get(i)));
    Handle<Object> result = ft.Call(test).ToHandleChecked();
    CHECK(result->IsString());
    CHECK(Handle<String>::cast(result)->IsFlat());
    CHECK(String::Equals(Handle<String>::cast(result), expected));
  }
}

TEST(TryToName) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 3;
  CodeStubAssemblerTester m(isolate, kNumParams);

  enum Result { kKeyIsIndex, kKeyIsUnique, kBailout };
  {
    Node* key = m.Parameter(0);
    Node* expected_result = m.Parameter(1);
    Node* expected_arg = m.Parameter(2);

    Label passed(&m), failed(&m);
    Label if_keyisindex(&m), if_keyisunique(&m), if_bailout(&m);
    Variable var_index(&m, MachineType::PointerRepresentation());

    m.TryToName(key, &if_keyisindex, &var_index, &if_keyisunique, &if_bailout);

    m.Bind(&if_keyisindex);
    m.GotoUnless(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kKeyIsIndex))),
        &failed);
    m.Branch(m.WordEqual(m.SmiUntag(expected_arg), var_index.value()), &passed,
             &failed);

    m.Bind(&if_keyisunique);
    m.GotoUnless(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kKeyIsUnique))),
        &failed);
    m.Branch(m.WordEqual(expected_arg, key), &passed, &failed);

    m.Bind(&if_bailout);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kBailout))),
        &passed, &failed);

    m.Bind(&passed);
    m.Return(m.BooleanConstant(true));

    m.Bind(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = m.GenerateCode();
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
}

namespace {

template <typename Dictionary>
void TestEntryToIndex() {
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  CodeStubAssemblerTester m(isolate, kNumParams);
  {
    Node* entry = m.SmiUntag(m.Parameter(0));
    Node* result = m.EntryToIndex<Dictionary>(entry);
    m.Return(m.SmiTag(result));
  }

  Handle<Code> code = m.GenerateCode();
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
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;
  CodeStubAssemblerTester m(isolate, kNumParams);

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
    m.Bind(&if_found);
    m.GotoUnless(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kFound))),
        &failed);
    m.Branch(m.Word32Equal(m.SmiToWord32(expected_arg), var_name_index.value()),
             &passed, &failed);

    m.Bind(&if_not_found);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kNotFound))),
        &passed, &failed);

    m.Bind(&passed);
    m.Return(m.BooleanConstant(true));

    m.Bind(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = m.GenerateCode();
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
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;
  CodeStubAssemblerTester m(isolate, kNumParams);

  enum Result { kFound, kNotFound };
  {
    Node* dictionary = m.Parameter(0);
    Node* key = m.SmiToWord32(m.Parameter(1));
    Node* expected_result = m.Parameter(2);
    Node* expected_arg = m.Parameter(3);

    Label passed(&m), failed(&m);
    Label if_found(&m), if_not_found(&m);
    Variable var_entry(&m, MachineType::PointerRepresentation());

    m.NumberDictionaryLookup<Dictionary>(dictionary, key, &if_found, &var_entry,
                                         &if_not_found);
    m.Bind(&if_found);
    m.GotoUnless(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kFound))),
        &failed);
    m.Branch(m.Word32Equal(m.SmiToWord32(expected_arg), var_entry.value()),
             &passed, &failed);

    m.Bind(&if_not_found);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kNotFound))),
        &passed, &failed);

    m.Bind(&passed);
    m.Return(m.BooleanConstant(true));

    m.Bind(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = m.GenerateCode();
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
  typedef CodeStubAssembler::Label Label;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;
  CodeStubAssemblerTester m(isolate, kNumParams);

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

    m.Bind(&if_found);
    m.Branch(m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kFound))),
             &passed, &failed);

    m.Bind(&if_not_found);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kNotFound))),
        &passed, &failed);

    m.Bind(&if_bailout);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kBailout))),
        &passed, &failed);

    m.Bind(&passed);
    m.Return(m.BooleanConstant(true));

    m.Bind(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = m.GenerateCode();
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
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());
  Factory* factory = isolate->factory();

  const int kNumParams = 2;
  CodeStubAssemblerTester m(isolate, kNumParams);

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

    m.Bind(&if_found);
    m.Return(var_value.value());

    m.Bind(&if_not_found);
    m.Return(m.HeapConstant(not_found_symbol));

    m.Bind(&if_bailout);
    m.Return(m.HeapConstant(bailout_symbol));
  }

  Handle<Code> code = m.GenerateCode();
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
  typedef CodeStubAssembler::Label Label;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 3;
  CodeStubAssemblerTester m(isolate, kNumParams);

  enum Result { kFound, kNotFound, kBailout };
  {
    Node* object = m.Parameter(0);
    Node* index = m.SmiToWord32(m.Parameter(1));
    Node* expected_result = m.Parameter(2);

    Label passed(&m), failed(&m);
    Label if_found(&m), if_not_found(&m), if_bailout(&m);

    Node* map = m.LoadMap(object);
    Node* instance_type = m.LoadMapInstanceType(map);

    m.TryLookupElement(object, map, instance_type, index, &if_found,
                       &if_not_found, &if_bailout);

    m.Bind(&if_found);
    m.Branch(m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kFound))),
             &passed, &failed);

    m.Bind(&if_not_found);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kNotFound))),
        &passed, &failed);

    m.Bind(&if_bailout);
    m.Branch(
        m.WordEqual(expected_result, m.SmiConstant(Smi::FromInt(kBailout))),
        &passed, &failed);

    m.Bind(&passed);
    m.Return(m.BooleanConstant(true));

    m.Bind(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Factory* factory = isolate->factory();
  Handle<Object> smi0(Smi::kZero, isolate);
  Handle<Object> smi1(Smi::FromInt(1), isolate);
  Handle<Object> smi7(Smi::FromInt(7), isolate);
  Handle<Object> smi13(Smi::FromInt(13), isolate);
  Handle<Object> smi42(Smi::FromInt(42), isolate);

  Handle<Object> expect_found(Smi::FromInt(kFound), isolate);
  Handle<Object> expect_not_found(Smi::FromInt(kNotFound), isolate);
  Handle<Object> expect_bailout(Smi::FromInt(kBailout), isolate);

#define CHECK_FOUND(object, index)                         \
  CHECK(JSReceiver::HasElement(object, index).FromJust()); \
  ft.CheckTrue(object, smi##index, expect_found);

#define CHECK_NOT_FOUND(object, index)                      \
  CHECK(!JSReceiver::HasElement(object, index).FromJust()); \
  ft.CheckTrue(object, smi##index, expect_not_found);

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

TEST(DeferredCodePhiHints) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Label block1(&m, Label::kDeferred);
  m.Goto(&block1);
  m.Bind(&block1);
  {
    Variable var_object(&m, MachineRepresentation::kTagged);
    Label loop(&m, &var_object);
    var_object.Bind(m.IntPtrConstant(0));
    m.Goto(&loop);
    m.Bind(&loop);
    {
      Node* map = m.LoadMap(var_object.value());
      var_object.Bind(map);
      m.Goto(&loop);
    }
  }
  CHECK(!m.GenerateCode().is_null());
}

TEST(TestOutOfScopeVariable) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());
  VoidDescriptor descriptor(isolate);
  CodeStubAssemblerTester m(isolate, descriptor);
  Label block1(&m);
  Label block2(&m);
  Label block3(&m);
  Label block4(&m);
  m.Branch(m.WordEqual(m.Parameter(0), m.IntPtrConstant(0)), &block1, &block4);
  m.Bind(&block4);
  {
    Variable var_object(&m, MachineRepresentation::kTagged);
    m.Branch(m.WordEqual(m.Parameter(0), m.IntPtrConstant(0)), &block2,
             &block3);

    m.Bind(&block2);
    var_object.Bind(m.IntPtrConstant(55));
    m.Goto(&block1);

    m.Bind(&block3);
    var_object.Bind(m.IntPtrConstant(66));
    m.Goto(&block1);
  }
  m.Bind(&block1);
  CHECK(!m.GenerateCode().is_null());
}

namespace {

void TestStubCacheOffsetCalculation(StubCache::Table table) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 2;
  CodeStubAssemblerTester m(isolate, kNumParams);

  {
    Node* name = m.Parameter(0);
    Node* map = m.Parameter(1);
    Node* primary_offset = m.StubCachePrimaryOffset(name, map);
    Node* result;
    if (table == StubCache::kPrimary) {
      result = primary_offset;
    } else {
      CHECK_EQ(StubCache::kSecondary, table);
      result = m.StubCacheSecondaryOffset(name, primary_offset);
    }
    m.Return(m.SmiFromWord32(result));
  }

  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(code, kNumParams);

  Factory* factory = isolate->factory();
  Handle<Name> names[] = {
      factory->NewSymbol(),
      factory->InternalizeUtf8String("a"),
      factory->InternalizeUtf8String("bb"),
      factory->InternalizeUtf8String("ccc"),
      factory->NewPrivateSymbol(),
      factory->InternalizeUtf8String("dddd"),
      factory->InternalizeUtf8String("eeeee"),
      factory->InternalizeUtf8String("name"),
      factory->NewSymbol(),
      factory->NewPrivateSymbol(),
  };

  Handle<Map> maps[] = {
      Handle<Map>(nullptr, isolate),
      factory->cell_map(),
      Map::Create(isolate, 0),
      factory->meta_map(),
      factory->code_map(),
      Map::Create(isolate, 0),
      factory->hash_table_map(),
      factory->symbol_map(),
      factory->string_map(),
      Map::Create(isolate, 0),
      factory->sloppy_arguments_elements_map(),
  };

  for (size_t name_index = 0; name_index < arraysize(names); name_index++) {
    Handle<Name> name = names[name_index];
    for (size_t map_index = 0; map_index < arraysize(maps); map_index++) {
      Handle<Map> map = maps[map_index];

      int expected_result;
      {
        int primary_offset = StubCache::PrimaryOffsetForTesting(*name, *map);
        if (table == StubCache::kPrimary) {
          expected_result = primary_offset;
        } else {
          expected_result =
              StubCache::SecondaryOffsetForTesting(*name, primary_offset);
        }
      }
      Handle<Object> result = ft.Call(name, map).ToHandleChecked();

      Smi* expected = Smi::FromInt(expected_result & Smi::kMaxValue);
      CHECK_EQ(expected, Smi::cast(*result));
    }
  }
}

}  // namespace

TEST(StubCachePrimaryOffset) {
  TestStubCacheOffsetCalculation(StubCache::kPrimary);
}

TEST(StubCacheSecondaryOffset) {
  TestStubCacheOffsetCalculation(StubCache::kSecondary);
}

namespace {

Handle<Code> CreateCodeWithFlags(Code::Flags flags) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  CodeStubAssemblerTester m(isolate, flags);
  m.Return(m.UndefinedConstant());
  return m.GenerateCodeCloseAndEscape();
}

}  // namespace

TEST(TryProbeStubCache) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());
  const int kNumParams = 3;
  CodeStubAssemblerTester m(isolate, kNumParams);

  Code::Kind ic_kind = Code::LOAD_IC;
  StubCache stub_cache(isolate, ic_kind);
  stub_cache.Clear();

  {
    Node* receiver = m.Parameter(0);
    Node* name = m.Parameter(1);
    Node* expected_handler = m.Parameter(2);

    Label passed(&m), failed(&m);

    Variable var_handler(&m, MachineRepresentation::kTagged);
    Label if_handler(&m), if_miss(&m);

    m.TryProbeStubCache(&stub_cache, receiver, name, &if_handler, &var_handler,
                        &if_miss);
    m.Bind(&if_handler);
    m.Branch(m.WordEqual(expected_handler, var_handler.value()), &passed,
             &failed);

    m.Bind(&if_miss);
    m.Branch(m.WordEqual(expected_handler, m.IntPtrConstant(0)), &passed,
             &failed);

    m.Bind(&passed);
    m.Return(m.BooleanConstant(true));

    m.Bind(&failed);
    m.Return(m.BooleanConstant(false));
  }

  Handle<Code> code = m.GenerateCode();
  FunctionTester ft(code, kNumParams);

  std::vector<Handle<Name>> names;
  std::vector<Handle<JSObject>> receivers;
  std::vector<Handle<Code>> handlers;

  base::RandomNumberGenerator rand_gen(FLAG_random_seed);

  Factory* factory = isolate->factory();

  // Generate some number of names.
  for (int i = 0; i < StubCache::kPrimaryTableSize / 7; i++) {
    Handle<Name> name;
    switch (rand_gen.NextInt(3)) {
      case 0: {
        // Generate string.
        std::stringstream ss;
        ss << "s" << std::hex
           << (rand_gen.NextInt(Smi::kMaxValue) % StubCache::kPrimaryTableSize);
        name = factory->InternalizeUtf8String(ss.str().c_str());
        break;
      }
      case 1: {
        // Generate number string.
        std::stringstream ss;
        ss << (rand_gen.NextInt(Smi::kMaxValue) % StubCache::kPrimaryTableSize);
        name = factory->InternalizeUtf8String(ss.str().c_str());
        break;
      }
      case 2: {
        // Generate symbol.
        name = factory->NewSymbol();
        break;
      }
      default:
        UNREACHABLE();
    }
    names.push_back(name);
  }

  // Generate some number of receiver maps and receivers.
  for (int i = 0; i < StubCache::kSecondaryTableSize / 2; i++) {
    Handle<Map> map = Map::Create(isolate, 0);
    receivers.push_back(factory->NewJSObjectFromMap(map));
  }

  // Generate some number of handlers.
  for (int i = 0; i < 30; i++) {
    Code::Flags flags =
        Code::RemoveHolderFromFlags(Code::ComputeHandlerFlags(ic_kind));
    handlers.push_back(CreateCodeWithFlags(flags));
  }

  // Ensure that GC does happen because from now on we are going to fill our
  // own stub cache instance with raw values.
  DisallowHeapAllocation no_gc;

  // Populate {stub_cache}.
  const int N = StubCache::kPrimaryTableSize + StubCache::kSecondaryTableSize;
  for (int i = 0; i < N; i++) {
    int index = rand_gen.NextInt();
    Handle<Name> name = names[index % names.size()];
    Handle<JSObject> receiver = receivers[index % receivers.size()];
    Handle<Code> handler = handlers[index % handlers.size()];
    stub_cache.Set(*name, receiver->map(), *handler);
  }

  // Perform some queries.
  bool queried_existing = false;
  bool queried_non_existing = false;
  for (int i = 0; i < N; i++) {
    int index = rand_gen.NextInt();
    Handle<Name> name = names[index % names.size()];
    Handle<JSObject> receiver = receivers[index % receivers.size()];
    Object* handler = stub_cache.Get(*name, receiver->map());
    if (handler == nullptr) {
      queried_non_existing = true;
    } else {
      queried_existing = true;
    }

    Handle<Object> expected_handler(handler, isolate);
    ft.CheckTrue(receiver, name, expected_handler);
  }

  for (int i = 0; i < N; i++) {
    int index1 = rand_gen.NextInt();
    int index2 = rand_gen.NextInt();
    Handle<Name> name = names[index1 % names.size()];
    Handle<JSObject> receiver = receivers[index2 % receivers.size()];
    Object* handler = stub_cache.Get(*name, receiver->map());
    if (handler == nullptr) {
      queried_non_existing = true;
    } else {
      queried_existing = true;
    }

    Handle<Object> expected_handler(handler, isolate);
    ft.CheckTrue(receiver, name, expected_handler);
  }
  // Ensure we performed both kind of queries.
  CHECK(queried_existing && queried_non_existing);
}

TEST(GotoIfException) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 1;
  // Emulate TFJ builtin
  CodeStubAssemblerTester m(isolate, kNumParams, Code::BUILTIN);

  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* to_string_tag =
      m.HeapConstant(isolate->factory()->to_string_tag_symbol());
  Variable exception(&m, MachineRepresentation::kTagged);

  Label exception_handler(&m);
  Callable to_string = CodeFactory::ToString(isolate);
  Node* string = m.CallStub(to_string, context, to_string_tag);
  m.GotoIfException(string, &exception_handler, &exception);
  m.Return(string);

  m.Bind(&exception_handler);
  m.Return(exception.value());

  Handle<Code> code = m.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result = ft.Call().ToHandleChecked();

  // Should be a TypeError
  CHECK(result->IsJSObject());

  Handle<Object> constructor =
      Object::GetPropertyOrElement(result,
                                   isolate->factory()->constructor_string())
          .ToHandleChecked();
  CHECK(constructor->SameValue(*isolate->type_error_function()));
}

TEST(GotoIfExceptionMultiple) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  Isolate* isolate(CcTest::InitIsolateOnce());

  const int kNumParams = 4;  // receiver, first, second, third
  // Emulate TFJ builtin
  CodeStubAssemblerTester m(isolate, kNumParams, Code::BUILTIN);

  Node* context = m.HeapConstant(Handle<Context>(isolate->native_context()));
  Node* first_value = m.Parameter(0);
  Node* second_value = m.Parameter(1);
  Node* third_value = m.Parameter(2);

  Label exception_handler1(&m);
  Label exception_handler2(&m);
  Label exception_handler3(&m);
  Variable return_value(&m, MachineRepresentation::kWord32);
  Variable error(&m, MachineRepresentation::kTagged);

  return_value.Bind(m.Int32Constant(0));

  // try { return ToString(param1) } catch (e) { ... }
  Callable to_string = CodeFactory::ToString(isolate);
  Node* string = m.CallStub(to_string, context, first_value);
  m.GotoIfException(string, &exception_handler1, &error);
  m.Return(string);

  // try { ToString(param2); return 7 } catch (e) { ... }
  m.Bind(&exception_handler1);
  return_value.Bind(m.Int32Constant(7));
  error.Bind(m.UndefinedConstant());
  string = m.CallStub(to_string, context, second_value);
  m.GotoIfException(string, &exception_handler2, &error);
  m.Return(m.SmiFromWord32(return_value.value()));

  // try { ToString(param3); return 7 & ~2; } catch (e) { return e; }
  m.Bind(&exception_handler2);
  // Return returnValue & ~2
  error.Bind(m.UndefinedConstant());
  string = m.CallStub(to_string, context, third_value);
  m.GotoIfException(string, &exception_handler3, &error);
  m.Return(m.SmiFromWord32(
      m.Word32And(return_value.value(),
                  m.Word32Xor(m.Int32Constant(2), m.Int32Constant(-1)))));

  m.Bind(&exception_handler3);
  m.Return(error.value());

  Handle<Code> code = m.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);

  Handle<Object> result;
  // First handler does not throw, returns result of first value
  result = ft.Call(isolate->factory()->undefined_value(),
                   isolate->factory()->to_string_tag_symbol())
               .ToHandleChecked();
  CHECK(String::cast(*result)->IsOneByteEqualTo(OneByteVector("undefined")));

  // First handler returns a number
  result = ft.Call(isolate->factory()->to_string_tag_symbol(),
                   isolate->factory()->undefined_value())
               .ToHandleChecked();
  CHECK_EQ(7, Smi::cast(*result)->value());

  // First handler throws, second handler returns a number
  result = ft.Call(isolate->factory()->to_string_tag_symbol(),
                   isolate->factory()->to_primitive_symbol())
               .ToHandleChecked();
  CHECK_EQ(7 & ~2, Smi::cast(*result)->value());

  // First handler throws, second handler throws, third handler returns thrown
  // value.
  result = ft.Call(isolate->factory()->to_string_tag_symbol(),
                   isolate->factory()->to_primitive_symbol(),
                   isolate->factory()->unscopables_symbol())
               .ToHandleChecked();

  // Should be a TypeError
  CHECK(result->IsJSObject());

  Handle<Object> constructor =
      Object::GetPropertyOrElement(result,
                                   isolate->factory()->constructor_string())
          .ToHandleChecked();
  CHECK(constructor->SameValue(*isolate->type_error_function()));
}

TEST(AllocateJSObjectFromMap) {
  Isolate* isolate(CcTest::InitIsolateOnce());
  Factory* factory = isolate->factory();

  const int kNumParams = 3;
  CodeStubAssemblerTester m(isolate, kNumParams);

  {
    Node* map = m.Parameter(0);
    Node* properties = m.Parameter(1);
    Node* elements = m.Parameter(2);

    Node* result = m.AllocateJSObjectFromMap(map, properties, elements);

    m.Return(result);
  }

  Handle<Code> code = m.GenerateCode();
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
  CodeStubAssemblerTester m(isolate, kNumParams);

  {
    Node* capacity = m.Parameter(0);
    Node* result = m.AllocateNameDictionary(m.SmiUntag(capacity));
    m.Return(result);
  }

  Handle<Code> code = m.GenerateCode();
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
  const int kNumProgramaticParams = 2;
  CodeStubAssemblerTester m(isolate, kNumParams - kNumProgramaticParams);

  // Call a function that return |kNumProgramaticParams| parameters in addition
  // to those specified by the static descriptor. |kNumProgramaticParams| is
  // specified as a constant.
  m.PopAndReturn(m.Int32Constant(kNumProgramaticParams),
                 m.SmiConstant(Smi::FromInt(1234)));

  Handle<Code> code = m.GenerateCode();
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
  const int kNumProgramaticParams = 2;
  CodeStubAssemblerTester m(isolate, kNumParams - kNumProgramaticParams);

  // Call a function that return |kNumProgramaticParams| parameters in addition
  // to those specified by the static descriptor. |kNumProgramaticParams| is
  // passed in as a parameter to the function so that it can't be recongized as
  // a constant.
  m.PopAndReturn(m.SmiUntag(m.Parameter(1)), m.SmiConstant(Smi::FromInt(1234)));

  Handle<Code> code = m.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result;
  for (int test_count = 0; test_count < 100; ++test_count) {
    result = ft.Call(isolate->factory()->undefined_value(),
                     Handle<Smi>(Smi::FromInt(1234), isolate),
                     isolate->factory()->undefined_value(),
                     Handle<Smi>(Smi::FromInt(kNumProgramaticParams), isolate))
                 .ToHandleChecked();
    CHECK_EQ(1234, Handle<Smi>::cast(result)->value());
  }
}

TEST(OneToTwoByteStringCopy) {
  Isolate* isolate(CcTest::InitIsolateOnce());

  CodeStubAssemblerTester m(isolate, 2);

  m.CopyStringCharacters(
      m.Parameter(0), m.Parameter(1), m.SmiConstant(Smi::FromInt(0)),
      m.SmiConstant(Smi::FromInt(0)), m.SmiConstant(Smi::FromInt(5)),
      String::ONE_BYTE_ENCODING, String::TWO_BYTE_ENCODING,
      CodeStubAssembler::SMI_PARAMETERS);
  m.Return(m.SmiConstant(Smi::FromInt(0)));

  Handle<Code> code = m.GenerateCode();
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

  CodeStubAssemblerTester m(isolate, 2);

  m.CopyStringCharacters(
      m.Parameter(0), m.Parameter(1), m.SmiConstant(Smi::FromInt(0)),
      m.SmiConstant(Smi::FromInt(0)), m.SmiConstant(Smi::FromInt(5)),
      String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING,
      CodeStubAssembler::SMI_PARAMETERS);
  m.Return(m.SmiConstant(Smi::FromInt(0)));

  Handle<Code> code = m.GenerateCode();
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

  CodeStubAssemblerTester m(isolate, 2);

  m.CopyStringCharacters(
      m.Parameter(0), m.Parameter(1), m.SmiConstant(Smi::FromInt(0)),
      m.SmiConstant(Smi::FromInt(3)), m.SmiConstant(Smi::FromInt(2)),
      String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING,
      CodeStubAssembler::SMI_PARAMETERS);
  m.Return(m.SmiConstant(Smi::FromInt(0)));

  Handle<Code> code = m.GenerateCode();
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

  CodeStubAssemblerTester m(isolate, 2);

  m.CopyStringCharacters(
      m.Parameter(0), m.Parameter(1), m.SmiConstant(Smi::FromInt(0)),
      m.SmiConstant(Smi::FromInt(0)), m.SmiConstant(Smi::FromInt(5)),
      String::TWO_BYTE_ENCODING, String::TWO_BYTE_ENCODING,
      CodeStubAssembler::SMI_PARAMETERS);
  m.Return(m.SmiConstant(Smi::FromInt(0)));

  Handle<Code> code = m.GenerateCode();
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
  CodeStubAssemblerTester m(isolate, kNumParams);

  CodeStubArguments arguments(&m, m.IntPtrConstant(3));

  CSA_ASSERT(
      &m, m.WordEqual(arguments.AtIndex(0), m.SmiConstant(Smi::FromInt(12))));
  CSA_ASSERT(
      &m, m.WordEqual(arguments.AtIndex(1), m.SmiConstant(Smi::FromInt(13))));
  CSA_ASSERT(
      &m, m.WordEqual(arguments.AtIndex(2), m.SmiConstant(Smi::FromInt(14))));

  m.Return(arguments.GetReceiver());

  Handle<Code> code = m.GenerateCode();
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
  CodeStubAssemblerTester m(isolate, kNumParams);

  CodeStubArguments arguments(&m, m.IntPtrConstant(3));

  CodeStubAssemblerTester::Variable sum(&m,
                                        MachineType::PointerRepresentation());
  CodeStubAssemblerTester::VariableList list({&sum}, m.zone());

  sum.Bind(m.IntPtrConstant(0));

  arguments.ForEach(list, [&m, &sum](CodeStubAssembler* assembler, Node* arg) {
    sum.Bind(assembler->IntPtrAdd(sum.value(), arg));
  });

  m.Return(sum.value());

  Handle<Code> code = m.GenerateCode();
  CHECK(!code.is_null());

  FunctionTester ft(code, kNumParams);
  Handle<Object> result = ft.Call(isolate->factory()->undefined_value(),
                                  Handle<Smi>(Smi::FromInt(12), isolate),
                                  Handle<Smi>(Smi::FromInt(13), isolate),
                                  Handle<Smi>(Smi::FromInt(14), isolate))
                              .ToHandleChecked();
  CHECK_EQ(Smi::FromInt(12 + 13 + 14), *result);
}

}  // namespace internal
}  // namespace v8
