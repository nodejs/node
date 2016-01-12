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

#include "v8.h"
#include "jsrtutils.h"

namespace v8 {

Local<Integer> Integer::New(Isolate* isolate, int32_t value) {
  return Local<Integer>::New(isolate, From(value));
}

Local<Integer> Integer::From(int32_t value) {
  JsValueRef ref;

  if (JsIntToNumber(value, &ref) != JsNoError) {
    return Local<Integer>();
  }

  // For perf reason, this doesn't allocate a real Handle
  return Local<Integer>(ref);
}

Local<Integer> Integer::NewFromUnsigned(Isolate* isolate, uint32_t value) {
  return Local<Integer>::New(isolate, From(value));
}

Local<Integer> Integer::From(uint32_t value) {
  JsValueRef ref;

  if (jsrt::UintToValue(value, &ref) != JsNoError) {
    return Local<Integer>();
  }

  // For perf reason, this doesn't allocate a real Handle
  return Local<Integer>(ref);
}

Integer* Integer::Cast(v8::Value* obj) {
  CHAKRA_ASSERT(obj->IsInt32());
  return static_cast<Integer*>(obj);
}

int64_t Integer::Value() const {
  return IntegerValue();
}

}  // namespace v8
