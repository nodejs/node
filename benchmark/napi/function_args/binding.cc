#include <v8.h>
#include <node.h>
#include <assert.h>

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

void CallWithString(const FunctionCallbackInfo<Value>& args) {
  assert(args.Length() == 1 && args[0]->IsString());
  if (args.Length() == 1 && args[0]->IsString()) {
    Local<String> str = args[0].As<String>();
    const int32_t length = str->Utf8Length(args.GetIsolate()) + 1;
    char* buf = new char[length];
    str->WriteUtf8(args.GetIsolate(), buf, length);
    delete[] buf;
  }
}

void CallWithArray(const FunctionCallbackInfo<Value>& args) {
  assert(args.Length() == 1 && args[0]->IsArray());
  if (args.Length() == 1 && args[0]->IsArray()) {
    const Local<Array> array = args[0].As<Array>();
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; i++) {
      Local<Value> v;
      v = array->Get(args.GetIsolate()->GetCurrentContext(),
                     i).ToLocalChecked();
    }
  }
}

void CallWithNumber(const FunctionCallbackInfo<Value>& args) {
  assert(args.Length() == 1 && args[0]->IsNumber());
  if (args.Length() == 1 && args[0]->IsNumber()) {
    args[0].As<Number>()->Value();
  }
}

void CallWithObject(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  assert(args.Length() == 1 && args[0]->IsObject());
  if (args.Length() == 1 && args[0]->IsObject()) {
    Local<Object> obj = args[0].As<Object>();

    MaybeLocal<String> map_key = String::NewFromUtf8(isolate,
        "map", v8::NewStringType::kNormal);
    assert(!map_key.IsEmpty());
    MaybeLocal<Value> map_maybe = obj->Get(context,
        map_key.ToLocalChecked());
    assert(!map_maybe.IsEmpty());
    Local<Value> map;
    map = map_maybe.ToLocalChecked();

    MaybeLocal<String> operand_key = String::NewFromUtf8(isolate,
        "operand", v8::NewStringType::kNormal);
    assert(!operand_key.IsEmpty());
    MaybeLocal<Value> operand_maybe = obj->Get(context,
        operand_key.ToLocalChecked());
    assert(!operand_maybe.IsEmpty());
    Local<Value> operand;
    operand = operand_maybe.ToLocalChecked();

    MaybeLocal<String> data_key = String::NewFromUtf8(isolate,
        "data", v8::NewStringType::kNormal);
    assert(!data_key.IsEmpty());
    MaybeLocal<Value> data_maybe = obj->Get(context,
        data_key.ToLocalChecked());
    assert(!data_maybe.IsEmpty());
    Local<Value> data;
    data = data_maybe.ToLocalChecked();

    MaybeLocal<String> reduce_key = String::NewFromUtf8(isolate,
        "reduce", v8::NewStringType::kNormal);
    assert(!reduce_key.IsEmpty());
    MaybeLocal<Value> reduce_maybe = obj->Get(context,
        reduce_key.ToLocalChecked());
    assert(!reduce_maybe.IsEmpty());
    Local<Value> reduce;
    reduce = reduce_maybe.ToLocalChecked();
  }
}

void CallWithTypedarray(const FunctionCallbackInfo<Value>& args) {
  assert(args.Length() == 1 && args[0]->IsArrayBufferView());
  if (args.Length() == 1 && args[0]->IsArrayBufferView()) {
    assert(args[0]->IsArrayBufferView());
    Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
    const size_t byte_offset = view->ByteOffset();
    const size_t byte_length = view->ByteLength();
    assert(byte_length > 0);
    assert(view->HasBuffer());
    Local<ArrayBuffer> buffer = view->Buffer();
    std::shared_ptr<BackingStore> bs = buffer->GetBackingStore();
    const uint32_t* data = reinterpret_cast<uint32_t*>(
        static_cast<uint8_t*>(bs->Data()) + byte_offset);
    assert(data);
  }
}

void CallWithArguments(const FunctionCallbackInfo<Value>& args) {
  assert(args.Length() > 1 && args[0]->IsNumber());
  if (args.Length() > 1 && args[0]->IsNumber()) {
    int32_t loop = args[0].As<Uint32>()->Value();
    for (int32_t i = 1; i < loop; ++i) {
      assert(i < args.Length());
      assert(args[i]->IsUint32());
      args[i].As<Uint32>()->Value();
    }
  }
}

void Initialize(Local<Object> target,
                Local<Value> module,
                void* data) {
  NODE_SET_METHOD(target, "callWithString", CallWithString);
  NODE_SET_METHOD(target, "callWithLongString", CallWithString);

  NODE_SET_METHOD(target, "callWithArray", CallWithArray);
  NODE_SET_METHOD(target, "callWithLargeArray", CallWithArray);
  NODE_SET_METHOD(target, "callWithHugeArray", CallWithArray);

  NODE_SET_METHOD(target, "callWithNumber", CallWithNumber);
  NODE_SET_METHOD(target, "callWithObject", CallWithObject);
  NODE_SET_METHOD(target, "callWithTypedarray", CallWithTypedarray);

  NODE_SET_METHOD(target, "callWith10Numbers", CallWithArguments);
  NODE_SET_METHOD(target, "callWith100Numbers", CallWithArguments);
  NODE_SET_METHOD(target, "callWith1000Numbers", CallWithArguments);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
