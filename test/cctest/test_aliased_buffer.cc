
#include "v8.h"
#include "aliased_buffer.h"
#include "node_test_fixture.h"

using node::AliasedBuffer;

class AliasBufferTest : public NodeTestFixture {};

template <class NativeT>
void CreateOracleValues(std::vector<NativeT>* buf) {
  for (size_t i = 0, j = buf->size(); i < buf->size(); i++, j--) {
    (*buf)[i] = static_cast<NativeT>(j);
  }
}

template <class NativeT, class V8T>
void WriteViaOperator(AliasedBuffer<NativeT, V8T>* aliasedBuffer,
                      const std::vector<NativeT>& oracle) {
  // write through the API
  for (size_t i = 0; i < oracle.size(); i++) {
    (*aliasedBuffer)[i] = oracle[i];
  }
}

template <class NativeT, class V8T>
void WriteViaSetValue(AliasedBuffer<NativeT, V8T>* aliasedBuffer,
                      const std::vector<NativeT>& oracle) {
  // write through the API
  for (size_t i = 0; i < oracle.size(); i++) {
    aliasedBuffer->SetValue(i, oracle[i]);
  }
}

template <class NativeT, class V8T>
void ReadAndValidate(v8::Isolate* isolate,
                     v8::Local<v8::Context> context,
                     AliasedBuffer<NativeT, V8T>* aliasedBuffer,
                     const std::vector<NativeT>& oracle) {
  // read through the API
  for (size_t i = 0; i < oracle.size(); i++) {
    NativeT v1 = (*aliasedBuffer)[i];
    NativeT v2 = aliasedBuffer->GetValue(i);
    EXPECT_TRUE(v1 == oracle[i]);
    EXPECT_TRUE(v2 == oracle[i]);
  }

  // validate size of JS Buffer
  EXPECT_TRUE(aliasedBuffer->GetJSArray()->Length() == oracle.size());
  EXPECT_TRUE(
    aliasedBuffer->GetJSArray()->ByteLength() ==
    (oracle.size() * sizeof(NativeT)));

  // validate operator * and GetBuffer are the same
  EXPECT_TRUE(aliasedBuffer->GetNativeBuffer() == *(*aliasedBuffer));

  // read through the JS API
  for (size_t i = 0; i < oracle.size(); i++) {
    v8::Local<V8T> v8TypedArray = aliasedBuffer->GetJSArray();
    v8::MaybeLocal<v8::Value> v = v8TypedArray->Get(context, i);
    EXPECT_TRUE(v.IsEmpty() == false);
    v8::Local<v8::Value> v2 = v.ToLocalChecked();
    EXPECT_TRUE(v2->IsNumber());
    v8::MaybeLocal<v8::Number> v3 = v2->ToNumber(context);
    v8::Local<v8::Number> v4 = v3.ToLocalChecked();
    NativeT actualValue = static_cast<NativeT>(v4->Value());
    EXPECT_TRUE(actualValue == oracle[i]);
  }
}

template <class NativeT, class V8T>
void ReadWriteTest(v8::Isolate* isolate) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  const size_t size = 100;
  AliasedBuffer<NativeT, V8T> ab(isolate, size);
  std::vector<NativeT> oracle(size);
  CreateOracleValues(&oracle);
  WriteViaOperator(&ab, oracle);
  ReadAndValidate(isolate, context, &ab, oracle);

  WriteViaSetValue(&ab, oracle);

  // validate copy constructor
  {
    AliasedBuffer<NativeT, V8T> ab2(ab);
    ReadAndValidate(isolate, context, &ab2, oracle);
  }
  ReadAndValidate(isolate, context, &ab, oracle);
}

template <
    class NativeT_A, class V8T_A,
    class NativeT_B, class V8T_B,
    class NativeT_C, class V8T_C>
void SharedBufferTest(
    v8::Isolate* isolate,
    size_t count_A,
    size_t count_B,
    size_t count_C) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  size_t sizeInBytes_A = count_A * sizeof(NativeT_A);
  size_t sizeInBytes_B = count_B * sizeof(NativeT_B);
  size_t sizeInBytes_C = count_C * sizeof(NativeT_C);

  AliasedBuffer<uint8_t, v8::Uint8Array> rootBuffer(
      isolate, sizeInBytes_A + sizeInBytes_B + sizeInBytes_C);
  AliasedBuffer<NativeT_A, V8T_A> ab_A(
      isolate, 0, count_A, rootBuffer);
  AliasedBuffer<NativeT_B, V8T_B> ab_B(
      isolate, sizeInBytes_A, count_B, rootBuffer);
  AliasedBuffer<NativeT_C, V8T_C> ab_C(
      isolate, sizeInBytes_A + sizeInBytes_B, count_C, rootBuffer);

  std::vector<NativeT_A> oracle_A(count_A);
  std::vector<NativeT_B> oracle_B(count_B);
  std::vector<NativeT_C> oracle_C(count_C);
  CreateOracleValues(&oracle_A);
  CreateOracleValues(&oracle_B);
  CreateOracleValues(&oracle_C);

  WriteViaOperator(&ab_A, oracle_A);
  WriteViaOperator(&ab_B, oracle_B);
  WriteViaOperator(&ab_C, oracle_C);

  ReadAndValidate(isolate, context, &ab_A, oracle_A);
  ReadAndValidate(isolate, context, &ab_B, oracle_B);
  ReadAndValidate(isolate, context, &ab_C, oracle_C);

  WriteViaSetValue(&ab_A, oracle_A);
  WriteViaSetValue(&ab_B, oracle_B);
  WriteViaSetValue(&ab_C, oracle_C);

  ReadAndValidate(isolate, context, &ab_A, oracle_A);
  ReadAndValidate(isolate, context, &ab_B, oracle_B);
  ReadAndValidate(isolate, context, &ab_C, oracle_C);
}

TEST_F(AliasBufferTest, Uint8Array) {
  ReadWriteTest<uint8_t, v8::Uint8Array>(isolate_);
}

TEST_F(AliasBufferTest, Int8Array) {
  ReadWriteTest<int8_t, v8::Int8Array>(isolate_);
}

TEST_F(AliasBufferTest, Uint16Array) {
  ReadWriteTest<uint16_t, v8::Uint16Array>(isolate_);
}

TEST_F(AliasBufferTest, Int16Array) {
  ReadWriteTest<int16_t, v8::Int16Array>(isolate_);
}

TEST_F(AliasBufferTest, Uint32Array) {
  ReadWriteTest<uint32_t, v8::Uint32Array>(isolate_);
}

TEST_F(AliasBufferTest, Int32Array) {
  ReadWriteTest<int32_t, v8::Int32Array>(isolate_);
}

TEST_F(AliasBufferTest, Float32Array) {
  ReadWriteTest<float, v8::Float32Array>(isolate_);
}

TEST_F(AliasBufferTest, Float64Array) {
  ReadWriteTest<double, v8::Float64Array>(isolate_);
}

TEST_F(AliasBufferTest, SharedArrayBuffer1) {
  SharedBufferTest<
      uint32_t, v8::Uint32Array,
      double, v8::Float64Array,
      int8_t, v8::Int8Array>(isolate_, 100, 80, 8);
}

TEST_F(AliasBufferTest, SharedArrayBuffer2) {
  SharedBufferTest<
      double, v8::Float64Array,
      int8_t, v8::Int8Array,
      double, v8::Float64Array>(isolate_, 100, 8, 8);
}

TEST_F(AliasBufferTest, SharedArrayBuffer3) {
  SharedBufferTest<
      int8_t, v8::Int8Array,
      int8_t, v8::Int8Array,
      double, v8::Float64Array>(isolate_, 1, 7, 8);
}

TEST_F(AliasBufferTest, SharedArrayBuffer4) {
  SharedBufferTest<
      int8_t, v8::Int8Array,
      int8_t, v8::Int8Array,
      int32_t, v8::Int32Array>(isolate_, 1, 3, 1);
}

TEST_F(AliasBufferTest, OperatorOverloads) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  const size_t size = 10;
  AliasedBuffer<uint32_t, v8::Uint32Array> ab{isolate_, size};

  EXPECT_EQ(static_cast<uint32_t>(1), ab[0] = 1);
  EXPECT_EQ(static_cast<uint32_t>(4), ab[0] += 3);
  EXPECT_EQ(static_cast<uint32_t>(2), ab[0] -= 2);
  EXPECT_EQ(static_cast<uint32_t>(-2), -ab[0]);
}

TEST_F(AliasBufferTest, OperatorOverloadsRefs) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  AliasedBuffer<uint32_t, v8::Uint32Array> ab{isolate_, 2};
  using Reference = AliasedBuffer<uint32_t, v8::Uint32Array>::Reference;
  Reference ref = ab[0];
  Reference ref_value = ab[1] = 2;

  EXPECT_EQ(static_cast<uint32_t>(2), ref = ref_value);
  EXPECT_EQ(static_cast<uint32_t>(4), ref += ref_value);
  EXPECT_EQ(static_cast<uint32_t>(2), ref -= ref_value);
  EXPECT_EQ(static_cast<uint32_t>(-2), -ref);
}
