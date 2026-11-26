// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_BINDINGS_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_BINDINGS_H_

#include <string>

#include "experimental/d8/mock-dom/dom.h"
#include "include/v8-function-callback.h"
#include "include/v8-template.h"
#include "src/base/macros.h"

namespace v8 {
namespace mock_dom {

using CallbackInfo = FunctionCallbackInfo<Value>;

// All mock DOM wrappers use one cppgc pointer tag; type checks go through the
// interface templates (receiver signatures and HasInstance).
constexpr CppHeapPointerTag kWrapperTag =
    CppHeapPointerTag::kFirstEmbedderWrappableTag;

Local<FunctionTemplate> InterfaceTemplate(Isolate* isolate,
                                          Interface interface);

// --- Defining interface members ---------------------------------------------

// Prototype method with a receiver signature, so V8 itself rejects calls on
// the wrong kind of object before the callback runs.
void DefineMethod(Isolate* isolate, Interface interface, const char* name,
                  FunctionCallback callback, int length = 0,
                  Local<Value> data = Local<Value>());
// Prototype accessor pair with a receiver signature.
void DefineAccessor(Isolate* isolate, Interface interface, const char* name,
                    FunctionCallback getter, FunctionCallback setter = nullptr,
                    Local<Value> data = Local<Value>());
void DefineStaticMethod(Isolate* isolate, Interface interface, const char* name,
                        FunctionCallback callback, int length = 0);
// Constant on both the interface object and its prototype.
void DefineConstant(Isolate* isolate, Interface interface, const char* name,
                    int value);
void SetConstructor(Isolate* isolate, Interface interface,
                    FunctionCallback callback, int length = 0);
// Members of the global object. They take no receiver signature: the
// implementation uses the realm the function belongs to.
void DefineGlobalMethod(Isolate* isolate, const char* name,
                        FunctionCallback callback, int length = 0);
void DefineGlobalAccessor(Isolate* isolate, const char* name,
                          FunctionCallback getter,
                          FunctionCallback setter = nullptr,
                          Local<Value> data = Local<Value>());

// --- Callback helpers --------------------------------------------------------

// The small integer a member was registered with as its callback data.
inline int DataInt(const CallbackInfo& info) {
  return static_cast<int>(info.Data().As<Integer>()->Value());
}

// The C++ object behind the receiver. Valid for members defined with a
// signature for an interface whose objects are T.
template <typename T>
T* Receiver(const CallbackInfo& info) {
  return static_cast<T*>(static_cast<ScriptWrappable*>(
      Object::Unwrap<kWrapperTag, Object::Wrappable>(info.GetIsolate(),
                                                     info.This())));
}
// Like Receiver, but also accepts the global object (a Window).
EventTarget* ReceiverEventTarget(const CallbackInfo& info);

void ThrowTypeError(Isolate* isolate, const std::string& message);
void ThrowDOMException(Isolate* isolate, const char* name,
                       const std::string& message);
// A DOMException object, for APIs that report errors as values.
Local<Value> NewDOMException(Isolate* isolate, const char* name,
                             const std::string& message);
// Throws the recorded exception, if any. Returns true if it threw.
bool ThrowIfFailed(Isolate* isolate, const ExceptionState& exception_state);
// "Illegal constructor" for interfaces that cannot be constructed from JS.
void IllegalConstructor(const CallbackInfo& info);
bool RequireConstructCall(const CallbackInfo& info);
bool RequireArguments(const CallbackInfo& info, int count);

// Web IDL DOMString conversion. Return false if the conversion threw.
bool ToString(const CallbackInfo& info, Local<Value> value, String* out);
bool ToAtom(const CallbackInfo& info, Local<Value> value, AtomicString* out);
bool ToStdString(const CallbackInfo& info, Local<Value> value,
                 std::string* out);
// [LegacyNullToEmptyString] and nullable variants.
bool ToStringNullAsEmpty(const CallbackInfo& info, Local<Value> value,
                         String* out);
bool ToStringOrNull(const CallbackInfo& info, Local<Value> value, String* out);
bool ToNumber(const CallbackInfo& info, Local<Value> value, double* out);
bool ToInt(const CallbackInfo& info, Local<Value> value, int* out);

// Returns the Node behind |value| or throws a TypeError naming |what|.
Node* ToNodeOrThrow(const CallbackInfo& info, Local<Value> value,
                    const char* what);

Local<v8::String> V8Name(Isolate* isolate, const char* name);
// Reads |name| from |object|. Returns false if the getter threw.
bool GetProperty(Isolate* isolate, Local<Object> object, const char* name,
                 Local<Value>* out);

template <typename T>
void Return(const CallbackInfo& info, T* object) {
  info.GetReturnValue().Set(ToV8(info.GetIsolate(), object));
}
inline void Return(const CallbackInfo& info, const String& string) {
  info.GetReturnValue().Set(ToV8String(info.GetIsolate(), string));
}
inline void ReturnOrNull(const CallbackInfo& info, const String& string) {
  info.GetReturnValue().Set(ToV8StringOrNull(info.GetIsolate(), string));
}

// --- Installation ------------------------------------------------------------

// Each bindings file contributes one of these; they are run once per isolate
// after all interface templates exist.
void InstallCoreBindings(Isolate* isolate);
void InstallElementBindings(Isolate* isolate);
void InstallWindowBindings(Isolate* isolate);
void InstallMiscBindings(Isolate* isolate);
void InstallCanvasBindings(Isolate* isolate);
void InstallRangeAndTraversalBindings(Isolate* isolate);
void InstallIndexedDBBindings(Isolate* isolate);
// Per-realm setup that cannot be expressed on templates.
void InitializeRealmForCore(Isolate* isolate, Local<Context> context);
void InitializeRealmForWindow(Isolate* isolate, Local<Context> context);
void InitializeRealmForTraversal(Isolate* isolate, Local<Context> context);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_BINDINGS_H_
