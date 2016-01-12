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

template <class Func>
static Local<Value> Utils::NewError(Handle<String> message, const Func& f) {
  JsValueRef value;
  if (f(*message, &value) != JsNoError) {
    return Local<Value>();
  }
  return Local<Value>::New(value);
}

Local<Value> Exception::RangeError(Handle<String> message) {
  return Utils::NewError(message, JsCreateRangeError);
}

Local<Value> Exception::ReferenceError(Handle<String> message) {
  return Utils::NewError(message, JsCreateReferenceError);
}

Local<Value> Exception::SyntaxError(Handle<String> message) {
  return Utils::NewError(message, JsCreateSyntaxError);
}

Local<Value> Exception::TypeError(Handle<String> message) {
  return Utils::NewError(message, JsCreateTypeError);
}

Local<Value> Exception::Error(Handle<String> message) {
  return Utils::NewError(message, JsCreateError);
}

}  // namespace v8
