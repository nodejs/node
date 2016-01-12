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
#include "v8-debug.h"
#include "jsrtutils.h"

namespace v8 {

__declspec(thread) bool g_EnableDebug = false;
static JsContextRef g_debugContext = JS_INVALID_REFERENCE;

bool Debug::EnableAgent(const char *name, int port, bool wait_for_connection) {
  HRESULT hr = S_OK;

#ifndef NODE_ENGINE_CHAKRACORE
  if (!g_EnableDebug) {
    // JsStartDebugging needs COM initialization
    IfComFailError(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    g_EnableDebug = true;

    Local<Context> currentContext = Context::GetCurrent();
    if (!currentContext.IsEmpty()) {
      // Turn on debug mode on current Context (script engine), which was
      // created before start debugging and not in debug mode.
      JsStartDebugging();
    }
  }

error:
#else
  hr = E_FAIL;  // ChakraCore does not support JsStartDebugging
#endif

  return SUCCEEDED(hr);
}

bool Debug::IsAgentEnabled() {
  return g_EnableDebug;
}

Local<Context> Debug::GetDebugContext() {
  if (g_debugContext == JS_INVALID_REFERENCE) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    g_debugContext = *Context::New(isolate);
    JsAddRef(g_debugContext, nullptr);
  }

  return static_cast<Context*>(g_debugContext);
}

void Debug::Dispose() {
  if (g_EnableDebug) {
    CoUninitialize();
  }
}

}  // namespace v8
