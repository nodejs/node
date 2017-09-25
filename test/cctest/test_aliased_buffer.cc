
#include "v8.h"
#include "aliased_buffer.h"
#include "node_test_fixture.h"

using node::AliasedBuffer;

class AliasBufferTest : public NodeTestFixture {
 protected:
  void SetUp() override {
    NodeTestFixture::SetUp();
  }

  void TearDown() override {
    NodeTestFixture::TearDown();
  }
};

template<class NativeT>
void CreateOracleValues(NativeT* buf, size_t count) {
  for (size_t i = 0, j = count; i < count; i++, j--) {
    buf[i] = static_cast<NativeT>(j);
  }
}

template<class NativeT, class V8T>
void WriteViaOperator(AliasedBuffer<NativeT, V8T>* aliasedBuffer,
                      size_t size,
                      NativeT* oracle) {
  // write through the API
  for (size_t i = 0; i < size; i++) {
    (*aliasedBuffer)[i] = oracle[i];
  }
}

template<class NativeT, class V8T>
void WriteViaSetValue(AliasedBuffer<NativeT, V8T>* aliasedBuffer,
                      size_t size,
                      NativeT* oracle) {
  // write through the API
  for (size_t i = 0; i < size; i++) {
    aliasedBuffer->SetValue(i, oracle[i]);
  }
}

template<class NativeT, class V8T>
void ReadAndValidate(v8::Isolate* isolate,
                     v8::Local<v8::Context> context,
                     AliasedBuffer<NativeT, V8T>* aliasedBuffer,
                     size_t size,
                     NativeT* oracle) {
  // read through the API
  for (size_t i = 0; i < size; i++) {
    NativeT v1 = (*aliasedBuffer)[i];
    NativeT v2 = aliasedBuffer->GetValue(i);
    EXPECT_TRUE(v1 == oracle[i]);
    EXPECT_TRUE(v2 == oracle[i]);
  }

  // validate size of JS Buffer
  EXPECT_TRUE(aliasedBuffer->GetJSArray()->Length() == size);
  EXPECT_TRUE(
    aliasedBuffer->GetJSArray()->ByteLength() ==
    (size * sizeof(NativeT)));

  // validate operator * and GetBuffer are the same
  EXPECT_TRUE(aliasedBuffer->GetNativeBuffer() == *(*aliasedBuffer));

  // read through the JS API
  for (size_t i = 0; i < size; i++) {
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

template<class NativeT, class V8T>
void ReadWriteTest(v8::Isolate* isolate) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  const size_t size = 100;
  AliasedBuffer<NativeT, V8T> ab(isolate, size);
  NativeT* oracle = new NativeT[size];
  CreateOracleValues(oracle, size);
  WriteViaOperator(&ab, size, oracle);
  ReadAndValidate(isolate, context, &ab, size, oracle);

  WriteViaSetValue(&ab, size, oracle);

  // validate copy constructor
  {
    AliasedBuffer<NativeT, V8T> ab2(ab);
    ReadAndValidate(isolate, context, &ab2, size, oracle);
  }
  ReadAndValidate(isolate, context, &ab, size, oracle);

  delete[] oracle;
}

template<
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

  NativeT_A* oracle_A = new NativeT_A[count_A];
  NativeT_B* oracle_B = new NativeT_B[count_B];
  NativeT_C* oracle_C = new NativeT_C[count_C];
  CreateOracleValues(oracle_A, count_A);
  CreateOracleValues(oracle_B, count_B);
  CreateOracleValues(oracle_C, count_C);

  WriteViaOperator(&ab_A, count_A, oracle_A);
  WriteViaOperator(&ab_B, count_B, oracle_B);
  WriteViaOperator(&ab_C, count_C, oracle_C);

  ReadAndValidate(isolate, context, &ab_A, count_A, oracle_A);
  ReadAndValidate(isolate, context, &ab_B, count_B, oracle_B);
  ReadAndValidate(isolate, context, &ab_C, count_C, oracle_C);

  WriteViaSetValue(&ab_A, count_A, oracle_A);
  WriteViaSetValue(&ab_B, count_B, oracle_B);
  WriteViaSetValue(&ab_C, count_C, oracle_C);

  ReadAndValidate(isolate, context, &ab_A, count_A, oracle_A);
  ReadAndValidate(isolate, context, &ab_B, count_B, oracle_B);
  ReadAndValidate(isolate, context, &ab_C, count_C, oracle_C);
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
