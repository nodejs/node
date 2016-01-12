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

MaybeLocal<String> Message::GetSourceLine(Local<Context> context) const {
  // CHAKRA-TODO: Figure out how to transmit this info...?
  return Local<String>();
}

Local<String> Message::GetSourceLine() const {
  return FromMaybe(GetSourceLine(Local<Context>()));
}

Handle<Value> Message::GetScriptResourceName() const {
  // CHAKRA-TODO: Figure out how to transmit this info...?
  return Handle<Value>();
}

Maybe<int> Message::GetLineNumber(Local<Context> context) const {
  // CHAKRA-TODO: Figure out how to transmit this info...?
  return Nothing<int>();
}

int Message::GetLineNumber() const {
  return FromMaybe(GetLineNumber(Local<Context>()));
}

Maybe<int> Message::GetStartColumn(Local<Context> context) const {
  // CHAKRA-TODO: Figure out how to transmit this info...?
  return Nothing<int>();
}

int Message::GetStartColumn() const {
  return FromMaybe(GetStartColumn(Local<Context>()));
}

Maybe<int> Message::GetEndColumn(Local<Context> context) const {
  // CHAKRA-TODO: Figure out how to transmit this info...?
  return Nothing<int>();
}

int Message::GetEndColumn() const {
  return FromMaybe(GetEndColumn(Local<Context>()));
}

}  // namespace v8
