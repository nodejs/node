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

using CachedPropertyIdRef = jsrt::CachedPropertyIdRef;
using jsrt::IsolateShim;
using jsrt::ContextShim;

Local<StackTrace> StackTrace::CurrentStackTrace(Isolate* isolate,
                                                int frame_limit,
                                                StackTraceOptions options) {
  IsolateShim* iso = IsolateShim::FromIsolate(isolate);
  ContextShim* contextShim = iso->GetCurrentContextShim();

  JsValueRef getStackTrace = contextShim->GetGetStackTraceFunction();
  JsValueRef stackTrace;
  if (jsrt::CallFunction(getStackTrace, &stackTrace) != JsNoError) {
    return Local<StackTrace>();
  }

  return static_cast<StackTrace*>(stackTrace);
}

Local<StackFrame> StackTrace::GetFrame(uint32_t index) const {
  JsValueRef frame;
  if (jsrt::GetIndexedProperty(const_cast<StackTrace*>(this),
                               static_cast<int>(index), &frame) != JsNoError) {
    return Local<StackFrame>();
  }

  return static_cast<StackFrame*>(frame);
}

int StackTrace::GetFrameCount() const {
  unsigned int length;
  if (jsrt::GetArrayLength(const_cast<StackTrace*>(this),
                           &length) != JsNoError) {
    return 0;
  }

  return static_cast<int>(length);
}

Local<Array> StackTrace::AsArray() {
  return Local<Array>(reinterpret_cast<Array*>(this));
}

int StackFrame::GetLineNumber() const {
  JsValueRef frame = const_cast<StackFrame*>(this);
  int result;
  if (jsrt::CallGetter(frame, CachedPropertyIdRef::getLineNumber,
                       &result) != JsNoError) {
    return 0;
  }
  return result;
}

int StackFrame::GetColumn() const {
  JsValueRef frame = const_cast<StackFrame*>(this);
  int result;
  if (jsrt::CallGetter(frame, CachedPropertyIdRef::getColumnNumber,
                       &result) != JsNoError) {
    return 0;
  }
  return result;
}

int StackFrame::GetScriptId() const {
  // CHAKRA-TODO
  return 0;
}

Local<String> StackFrame::GetScriptName() const {
  JsValueRef frame = const_cast<StackFrame*>(this);
  JsValueRef result;
  if (jsrt::CallGetter(frame, CachedPropertyIdRef::getFileName,
                       &result) != JsNoError) {
    return Local<String>();
  }
  return static_cast<String*>(result);
}

Local<String> StackFrame::GetFunctionName() const {
  JsValueRef frame = const_cast<StackFrame*>(this);
  JsValueRef result;
  if (jsrt::CallGetter(frame, CachedPropertyIdRef::getFunctionName,
                       &result) != JsNoError) {
    return Local<String>();
  }
  return static_cast<String*>(result);
}

bool StackFrame::IsEval() const {
  // CHAKRA-TODO
  return false;
}

}  // namespace v8
