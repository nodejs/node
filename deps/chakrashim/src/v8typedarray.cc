// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "v8chakra.h"
#include "jsrtutils.h"

namespace v8 {

using jsrt::IsolateShim;
using jsrt::ContextShim;
using jsrt::CachedPropertyIdRef;

Local<ArrayBuffer> ArrayBufferView::Buffer() {
  JsValueRef result;
  if (JsGetTypedArrayInfo(this,
                          nullptr, &result, nullptr, nullptr) != JsNoError) {
    return Local<ArrayBuffer>();
  }
  return static_cast<ArrayBuffer*>(result);
}

size_t ArrayBufferView::ByteOffset() {
  unsigned int result;
  if (JsGetTypedArrayInfo(this,
                          nullptr, nullptr, &result, nullptr) != JsNoError) {
    return 0;
  }
  return result;
}

size_t ArrayBufferView::ByteLength() {
  unsigned int result;
  if (JsGetTypedArrayInfo(this,
                          nullptr, nullptr, nullptr, &result) != JsNoError) {
    return 0;
  }
  return result;
}

JsErrorCode Utils::NewTypedArray(ContextShim::GlobalType constructorIndex,
                                 Handle<ArrayBuffer> array_buffer,
                                 size_t byte_offset, size_t length,
                                 JsValueRef* result) {
  Isolate* iso = Isolate::GetCurrent();
  HandleScope handleScope(iso);

  ContextShim* contextShim = ContextShim::GetCurrent();
  JsValueRef constructor = contextShim->GetGlobalType(constructorIndex);
  JsValueRef args[4] = {
    contextShim->GetUndefined(),
    *array_buffer,
    *Integer::From(static_cast<uint32_t>(byte_offset)),
    *Integer::From(static_cast<uint32_t>(length))
  };
  return JsConstructObject(constructor, args, _countof(args), result);
}

template <class T>
Local<T> Utils::NewTypedArray(ContextShim::GlobalType constructorIndex,
                              Handle<ArrayBuffer> array_buffer,
                              size_t byte_offset, size_t length) {
  JsValueRef result;
  if (NewTypedArray(constructorIndex, array_buffer, byte_offset, length,
                    &result) != JsNoError) {
    return Local<T>();
  }
  return Local<T>::New(result);
}

#define DEFINE_TYPEDARRAY_NEW(ArrayType) \
  Local<ArrayType##Array> v8::ArrayType##Array::New(  \
                        Handle<ArrayBuffer> array_buffer, size_t byte_offset, \
                        size_t length) {  \
    return Utils::NewTypedArray<ArrayType##Array>( \
             ContextShim::GlobalType::ArrayType##Array, \
             array_buffer, byte_offset, length);  \
}


#define DEFINE_TYPEDARRAY_CAST(ArrayType) \
  ArrayType##Array* ArrayType##Array::Cast(Value* obj) {  \
    CHAKRA_ASSERT(obj->Is##ArrayType##Array()); \
    return static_cast<ArrayType##Array*>(obj); \
}

DEFINE_TYPEDARRAY_NEW(Uint8)
DEFINE_TYPEDARRAY_CAST(Uint8)

DEFINE_TYPEDARRAY_NEW(Uint8Clamped)
DEFINE_TYPEDARRAY_CAST(Uint8Clamped)

DEFINE_TYPEDARRAY_NEW(Int8)
DEFINE_TYPEDARRAY_CAST(Int8)

DEFINE_TYPEDARRAY_NEW(Uint16)
DEFINE_TYPEDARRAY_CAST(Uint16)

DEFINE_TYPEDARRAY_NEW(Int16)
DEFINE_TYPEDARRAY_CAST(Int16)

DEFINE_TYPEDARRAY_NEW(Uint32)
DEFINE_TYPEDARRAY_CAST(Uint32)

DEFINE_TYPEDARRAY_NEW(Int32)
DEFINE_TYPEDARRAY_CAST(Int32)

DEFINE_TYPEDARRAY_NEW(Float32)
DEFINE_TYPEDARRAY_CAST(Float32)

DEFINE_TYPEDARRAY_NEW(Float64)
DEFINE_TYPEDARRAY_CAST(Float64)
}  // namespace v8
