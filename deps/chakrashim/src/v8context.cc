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
#include <assert.h>

namespace v8 {

Context::Scope::Scope(Handle<Context> context) {
  jsrt::IsolateShim * isolateShim = jsrt::IsolateShim::GetCurrent();
  assert(isolateShim != nullptr);
  isolateShim->PushScope(
    reinterpret_cast<jsrt::ContextShim::Scope  *>(this),
    (JsContextRef)*context);
}

Context::Scope::~Scope() {
  jsrt::IsolateShim::GetCurrent()->PopScope(
    reinterpret_cast<jsrt::ContextShim::Scope *>(this));
}

Local<Object> Context::Global() {
  // V8 Global is actually proxy where the actual global is it's prototype.
  // No need to create handle here, the context will keep it alive
  return Local<Object>(static_cast<Object *>(
    jsrt::ContextShim::GetCurrent()->GetProxyOfGlobal()));
}

extern bool g_exposeGC;

Local<Context> Context::New(Isolate* external_isolate,
                            ExtensionConfiguration* extensions,
                            Handle<ObjectTemplate> global_template,
                            Handle<Value> global_object) {
  // CHAKRA-TODO: extensions and global_object parameter is not implemented
  if (extensions != nullptr || *global_object != JS_INVALID_REFERENCE) {
    CHAKRA_UNIMPLEMENTED_(
    "Context::New with externsions/global_object argument");
    return Local<Context>();
  }

  Local<Object> glob;
  if (!global_template.IsEmpty()) {
    glob = global_template->NewInstance();
    if (glob.IsEmpty()) {
      return Local<Context>();
    }
  }

  JsContextRef context;
  jsrt::IsolateShim* isoShim = jsrt::IsolateShim::FromIsolate(external_isolate);
  if (!isoShim->NewContext(&context, g_exposeGC, *glob)) {
    return Local<Context>();
  }

  return Local<Context>::New(external_isolate, static_cast<Context *>(context));
}

Local<Context> Context::GetCurrent() {
  JsContextRef current = jsrt::ContextShim::GetCurrent()->GetContextRef();

  // Don't need to create a ref here, because a context can't keep itself alive
  // anyway.
  return Local<Context>(static_cast<Context *>(current));
}

Isolate* Context::GetIsolate() {
  return Isolate::GetCurrent();
}

void* Context::GetAlignedPointerFromEmbedderData(int index) {
  jsrt::ContextShim * contextShim =
    jsrt::IsolateShim::GetCurrent()->GetContextShim((JsContextRef)this);
  return contextShim->GetAlignedPointerFromEmbedderData(index);
}

void Context::SetAlignedPointerInEmbedderData(int index, void* value) {
  jsrt::ContextShim * contextShim =
    jsrt::IsolateShim::GetCurrent()->GetContextShim((JsContextRef)this);
  return contextShim->SetAlignedPointerInEmbedderData(index, value);
}

void Context::SetSecurityToken(Handle<Value> token) {
  // CHAKRA-TODO
}

Handle<Value> Context::GetSecurityToken() {
  // CHAKRA-TODO
  return Handle<Value>();
}

}  // namespace v8
