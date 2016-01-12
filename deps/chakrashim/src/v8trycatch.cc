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

TryCatch::TryCatch(Isolate* isolate)
    : error(JS_INVALID_REFERENCE),
      rethrow(false),
      verbose(false) {
  jsrt::IsolateShim * isolateShim = jsrt::IsolateShim::GetCurrent();
  prev = isolateShim->tryCatchStackTop;
  isolateShim->tryCatchStackTop = this;
}

TryCatch::~TryCatch() {
  if (!rethrow) {
    GetAndClearException();
  }

  jsrt::IsolateShim::GetCurrent()->tryCatchStackTop = prev;
}

bool TryCatch::HasCaught() const {
  if (error == JS_INVALID_REFERENCE) {
    const_cast<TryCatch*>(this)->GetAndClearException();
  }

  if (error != JS_INVALID_REFERENCE) {
    return true;
  }
  bool hasException;
  JsErrorCode errorCode = JsHasException(&hasException);
  if (errorCode != JsNoError) {
    if (errorCode == JsErrorInDisabledState) {
      return true;
    }
    // Should never get errorCode other than JsNoError/JsErrorInDisabledState
    CHAKRA_ASSERT(false);
    return false;
  }

  return hasException;
}

bool TryCatch::HasTerminated() const {
  return jsrt::IsolateShim::GetCurrent()->IsExeuctionDisabled();
}

void TryCatch::GetAndClearException() {
  bool hasException;
  JsErrorCode errorCode = JsHasException(&hasException);
  if (errorCode != JsNoError) {
    // If script is in disabled state, no need to fail here.
    // We will fail subsequent calls to Jsrt anyway.
    CHAKRA_ASSERT(errorCode == JsErrorInDisabledState);
    return;
  }

  if (hasException) {
    JsValueRef exceptionRef;
    errorCode = JsGetAndClearException(&exceptionRef);
    // We came here through JsHasException, so script shouldn't be in disabled
    // state.
    CHAKRA_ASSERT(errorCode != JsErrorInDisabledState);
    if (errorCode == JsNoError) {
      error = exceptionRef;
    }
  }
}

Handle<Value> TryCatch::ReThrow() {
  if (error == JS_INVALID_REFERENCE) {
    GetAndClearException();
  }

  if (error == JS_INVALID_REFERENCE) {
    return Local<Value>();
  }

  if (JsSetException(error) != JsNoError) {
    return Handle<Value>();
  }
  rethrow = true;

  return Local<Value>::New(error);
}

Local<Value> TryCatch::Exception() const {
  if (error == JS_INVALID_REFERENCE) {
    const_cast<TryCatch*>(this)->GetAndClearException();
  }

  if (error == JS_INVALID_REFERENCE) {
    return Local<Value>();
  }

  return Local<Value>::New(error);
}

MaybeLocal<Value> TryCatch::StackTrace(Local<Context> context) const {
  if (error == JS_INVALID_REFERENCE) {
    const_cast<TryCatch*>(this)->GetAndClearException();
  }

  if (error == JS_INVALID_REFERENCE) {
    return Local<Value>();
  }

  JsPropertyIdRef stack = jsrt::IsolateShim::GetCurrent()
    ->GetCachedPropertyIdRef(jsrt::CachedPropertyIdRef::stack);
  if (stack == JS_INVALID_REFERENCE) {
    return Local<Value>();
  }

  JsValueRef trace;
  if (JsGetProperty(error, stack, &trace) != JsNoError) {
    return Local<Value>();
  }

  return Local<Value>::New(trace);
}

Local<Value> TryCatch::StackTrace() const {
  return FromMaybe(StackTrace(Local<Context>()));
}

Local<v8::Message> TryCatch::Message() const {
  // return an empty ref for now, so no nulls/empty messages will be printed
  // should be changed once we understand how to retreive the info for each
  // errror message
  return Local<v8::Message>();
}

void TryCatch::SetVerbose(bool value) {
  this->verbose = value;
}

void TryCatch::CheckReportExternalException() {
  // This is only used by Function::Call. If caller does not use TryCatch to
  // handle external exceptions, or uses a TryCatch and SetVerbose(),
  // we'll report the external exception message (triggers uncaughtException).
  if (prev == nullptr || prev->verbose) {
    jsrt::IsolateShim::GetCurrent()->ForEachMessageListener([this](
        void * messageListener) {
      ((v8::MessageCallback)messageListener)(Message(), Exception());
    });
  } else {
    rethrow = true;  // Otherwise leave the exception as is
  }
}

}  // namespace v8
