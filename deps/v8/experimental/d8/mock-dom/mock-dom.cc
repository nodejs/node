// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/d8/mock-dom/mock-dom.h"

#include "experimental/d8/mock-dom/bindings.h"
#include "experimental/d8/mock-dom/html.h"
#include "experimental/d8/mock-dom/isolate-data.h"
#include "experimental/d8/mock-dom/objects.h"
#include "include/v8-context.h"
#include "include/v8-primitive.h"
#include "include/v8-template.h"

namespace v8 {
namespace mock_dom {

Local<ObjectTemplate> CreateGlobalTemplate(Isolate* isolate) {
  IsolateData::Create(isolate);
  IsolateData* data = IsolateData::Get();
  for (int i = 0; i < kInterfaceCount; ++i) {
    Interface interface = static_cast<Interface>(i);
    Local<FunctionTemplate> tmpl =
        FunctionTemplate::New(isolate, IllegalConstructor);
    tmpl->SetClassName(V8Name(isolate, InterfaceName(interface)));
    Interface parent = InterfaceParent(interface);
    if (parent != Interface::kNone) {
      tmpl->Inherit(data->templates[static_cast<int>(parent)].Get(isolate));
    }
    tmpl->ReadOnlyPrototype();
    tmpl->PrototypeTemplate()->Set(
        Symbol::GetToStringTag(isolate),
        V8Name(isolate, InterfaceName(interface)),
        static_cast<PropertyAttribute>(ReadOnly | DontEnum));
    data->templates[i].Reset(isolate, tmpl);
  }
  InstallCoreBindings(isolate);
  InstallElementBindings(isolate);
  InstallWindowBindings(isolate);
  InstallMiscBindings(isolate);
  InstallCanvasBindings(isolate);
  InstallRangeAndTraversalBindings(isolate);
  InstallIndexedDBBindings(isolate);
  return InterfaceTemplate(isolate, Interface::kWindow)->InstanceTemplate();
}

void InitializeContext(Isolate* isolate, Local<Context> context) {
  HandleScope handle_scope(isolate);
  Context::Scope context_scope(context);
  Window* window = MakeGarbageCollected<Window>(isolate, isolate, context);
  // The global object itself cannot carry the C++ pointer, so a plain holder
  // object does, kept alive by the context.
  Local<Object> holder = InterfaceTemplate(isolate, Interface::kEventTarget)
                             ->InstanceTemplate()
                             ->NewInstance(context)
                             .ToLocalChecked();
  Object::Wrap<kWrapperTag>(isolate, holder, window);
  Window::Install(isolate, context, holder, window);
  window->Initialize(isolate);
  InitializeRealmForCore(isolate, context);
  InitializeRealmForWindow(isolate, context);
  InitializeRealmForTraversal(isolate, context);
  // Start with an empty but well-formed document, like about:blank.
  ParseHTMLDocument(isolate, window->document(), "");
}

bool ReportException(Isolate* isolate, Local<Context> context,
                     Local<Value> exception, Local<Message> message) {
  Window* window = Window::From(context);
  if (!window) return false;
  window->ReportException(isolate, exception, message);
  return true;
}

bool ShouldRunTasks(Local<Context> context) {
  Window* window = Window::From(context);
  return !window || !window->detached;
}

void Shutdown(Isolate* isolate) { IsolateData::Destroy(isolate); }

}  // namespace mock_dom
}  // namespace v8
