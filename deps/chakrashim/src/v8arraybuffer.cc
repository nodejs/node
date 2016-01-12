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
using jsrt::CachedPropertyIdRef;

Local<ArrayBuffer> ArrayBuffer::New(Isolate* isolate, size_t byte_length) {
  JsValueRef result;
  if (JsCreateArrayBuffer(static_cast<unsigned int>(byte_length),
                          &result) != JsNoError) {
    return Local<ArrayBuffer>();
  }
  return Local<ArrayBuffer>::New(result);
}

struct ArrayBufferFinalizeInfo {
  ArrayBuffer::Allocator* allocator;
  void *data;
  size_t length;

  void Free() {
    allocator->Free(data, length);
    delete this;
  }
};

static void CALLBACK ExternalArrayBufferFinalizeCallback(void *data) {
    static_cast<ArrayBufferFinalizeInfo*>(data)->Free();
}

Local<ArrayBuffer> ArrayBuffer::New(Isolate* isolate,
                                    void* data, size_t byte_length,
                                    ArrayBufferCreationMode mode) {
  JsFinalizeCallback finalizeCallback = nullptr;
  ArrayBufferFinalizeInfo* callbackState = nullptr;

  if (mode == ArrayBufferCreationMode::kInternalized) {
      ArrayBufferFinalizeInfo info = { g_arrayBufferAllocator,
                                       data,
                                       byte_length };
      finalizeCallback = ExternalArrayBufferFinalizeCallback;
      callbackState = new ArrayBufferFinalizeInfo(info);
  }

  JsValueRef result;
  if (JsCreateExternalArrayBuffer(data,
                                  static_cast<unsigned int>(byte_length),
                                  finalizeCallback,
                                  callbackState,
                                  &result) != JsNoError) {
    if (callbackState != nullptr) {
      delete callbackState;
    }
    return Local<ArrayBuffer>();
  }
  return Local<ArrayBuffer>::New(result);
}

size_t ArrayBuffer::ByteLength() const {
  BYTE* buffer;
  unsigned int length;
  if (JsGetArrayBufferStorage(const_cast<ArrayBuffer*>(this),
                              &buffer, &length) != JsNoError) {
    return 0;
  }
  return length;
}

void ArrayBuffer::Neuter() {
  // Chakra: not supported, ignore
}

ArrayBuffer::Contents ArrayBuffer::GetContents() {
  BYTE* buffer;
  unsigned int bufferLength;
  if (JsGetArrayBufferStorage(this, &buffer, &bufferLength) != JsNoError) {
    return Contents();
  }

  Contents contents;
  contents.data_ = buffer;
  contents.byte_length_ = bufferLength;
  return contents;
}

ArrayBuffer* ArrayBuffer::Cast(Value* obj) {
  CHAKRA_ASSERT(obj->IsArrayBuffer());
  return static_cast<ArrayBuffer*>(obj);
}

}  // namespace v8
