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

namespace v8 {

using jsrt::IsolateShim;
using jsrt::ContextShim;

MaybeLocal<Value> Date::New(Local<Context> context, double time) {
  JsValueRef dateConstructor = ContextShim::GetCurrent()->GetDateConstructor();

  JsValueRef numberRef;
  if (JsDoubleToNumber(time, &numberRef) != JsNoError) {
    return Local<Value>();
  }

  JsValueRef newDateRef;
  if (jsrt::ConstructObject(dateConstructor,
                            numberRef, &newDateRef) != JsNoError) {
    return Local<Value>();
  }

  return Local<Date>::New(newDateRef);
}

Local<Value> Date::New(Isolate * isolate, double time) {
  return FromMaybe(New(Local<Context>(), time));
}

// Not Implemented
Date *Date::Cast(v8::Value *obj) {
  CHAKRA_ASSERT(obj->IsDate());
  return static_cast<Date*>(obj);
}

}  // namespace v8
