// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bindings infrastructure plus EventTarget, Event, Node, CharacterData,
// Document, DocumentFragment and the node collections.

#include <cmath>

#include "experimental/d8/mock-dom/bindings.h"
#include "experimental/d8/mock-dom/css.h"
#include "experimental/d8/mock-dom/html.h"
#include "experimental/d8/mock-dom/isolate-data.h"
#include "experimental/d8/mock-dom/objects.h"
#include "include/v8-container.h"
#include "include/v8-exception.h"
#include "include/v8-external.h"
#include "include/v8-primitive.h"
#include "include/v8-proxy.h"
#include "src/api/api-inl.h"
#include "src/objects/js-objects-inl.h"

namespace v8 {
namespace mock_dom {

// --- Infrastructure ----------------------------------------------------------

Local<FunctionTemplate> InterfaceTemplate(Isolate* isolate,
                                          Interface interface) {
  return IsolateData::Get()->templates[static_cast<int>(interface)].Get(
      isolate);
}

Local<v8::String> V8Name(Isolate* isolate, const char* name) {
  return v8::String::NewFromUtf8(isolate, name, NewStringType::kInternalized)
      .ToLocalChecked();
}

void DefineMethod(Isolate* isolate, Interface interface, const char* name,
                  FunctionCallback callback, int length, Local<Value> data) {
  Local<FunctionTemplate> tmpl = InterfaceTemplate(isolate, interface);
  Local<FunctionTemplate> function = FunctionTemplate::New(
      isolate, callback, data, Signature::New(isolate, tmpl), length,
      ConstructorBehavior::kThrow);
  tmpl->PrototypeTemplate()->Set(V8Name(isolate, name), function);
}

void DefineAccessor(Isolate* isolate, Interface interface, const char* name,
                    FunctionCallback getter, FunctionCallback setter,
                    Local<Value> data) {
  Local<FunctionTemplate> tmpl = InterfaceTemplate(isolate, interface);
  Local<Signature> signature = Signature::New(isolate, tmpl);
  Local<FunctionTemplate> getter_template = FunctionTemplate::New(
      isolate, getter, data, signature, 0, ConstructorBehavior::kThrow);
  Local<FunctionTemplate> setter_template;
  if (setter) {
    setter_template = FunctionTemplate::New(isolate, setter, data, signature, 1,
                                            ConstructorBehavior::kThrow);
  }
  tmpl->PrototypeTemplate()->SetAccessorProperty(
      V8Name(isolate, name), getter_template, setter_template);
}

void DefineStaticMethod(Isolate* isolate, Interface interface, const char* name,
                        FunctionCallback callback, int length) {
  Local<FunctionTemplate> tmpl = InterfaceTemplate(isolate, interface);
  tmpl->Set(V8Name(isolate, name),
            FunctionTemplate::New(isolate, callback, Local<Value>(),
                                  Local<Signature>(), length,
                                  ConstructorBehavior::kThrow));
}

void DefineConstant(Isolate* isolate, Interface interface, const char* name,
                    int value) {
  Local<FunctionTemplate> tmpl = InterfaceTemplate(isolate, interface);
  Local<Integer> v = Integer::New(isolate, value);
  PropertyAttribute attributes =
      static_cast<PropertyAttribute>(ReadOnly | DontDelete);
  tmpl->Set(V8Name(isolate, name), v, attributes);
  tmpl->PrototypeTemplate()->Set(V8Name(isolate, name), v, attributes);
}

void SetConstructor(Isolate* isolate, Interface interface,
                    FunctionCallback callback, int length) {
  Local<FunctionTemplate> tmpl = InterfaceTemplate(isolate, interface);
  tmpl->SetCallHandler(callback,
                       Integer::New(isolate, static_cast<int>(interface)));
  tmpl->SetLength(length);
}

void DefineGlobalMethod(Isolate* isolate, const char* name,
                        FunctionCallback callback, int length) {
  Local<FunctionTemplate> tmpl = InterfaceTemplate(isolate, Interface::kWindow);
  tmpl->InstanceTemplate()->Set(
      V8Name(isolate, name),
      FunctionTemplate::New(isolate, callback, Local<Value>(),
                            Local<Signature>(), length,
                            ConstructorBehavior::kThrow));
}

void DefineGlobalAccessor(Isolate* isolate, const char* name,
                          FunctionCallback getter, FunctionCallback setter,
                          Local<Value> data) {
  Local<FunctionTemplate> tmpl = InterfaceTemplate(isolate, Interface::kWindow);
  Local<FunctionTemplate> getter_template =
      FunctionTemplate::New(isolate, getter, data, Local<Signature>(), 0,
                            ConstructorBehavior::kThrow);
  Local<FunctionTemplate> setter_template;
  if (setter) {
    setter_template =
        FunctionTemplate::New(isolate, setter, data, Local<Signature>(), 1,
                              ConstructorBehavior::kThrow);
  }
  tmpl->InstanceTemplate()->SetAccessorProperty(
      V8Name(isolate, name), getter_template, setter_template);
}

namespace {

bool IsGlobalProxy(Local<Object> object) {
  return i::IsJSGlobalProxy(*Utils::OpenDirectHandle(*object));
}

Window* WindowFromGlobalProxy(Isolate* isolate, Local<Object> object) {
  Local<Context> context;
  if (!object->GetCreationContext(isolate).ToLocal(&context)) return nullptr;
  return Window::From(context);
}

}  // namespace

Local<Value> ToV8(Isolate* isolate, ScriptWrappable* object) {
  if (!object) return Null(isolate);
  if (object->has_wrapper()) return object->wrapper(isolate);
  if (object->interface() == Interface::kWindow) {
    return static_cast<Window*>(object)->context(isolate)->Global();
  }
  Window* window = object->WrapperWindow();
  Local<Context> context =
      window ? window->context(isolate) : isolate->GetCurrentContext();
  Local<Object> wrapper = InterfaceTemplate(isolate, object->interface())
                              ->InstanceTemplate()
                              ->NewInstance(context)
                              .ToLocalChecked();
  Object::Wrap<kWrapperTag>(isolate, wrapper, object);
  object->set_wrapper(isolate, wrapper);
  return wrapper;
}

ScriptWrappable* ToWrappable(Isolate* isolate, Local<Value> value,
                             Interface interface) {
  if (!value->IsObject()) return nullptr;
  Local<Object> object = value.As<Object>();
  if (IsGlobalProxy(object)) {
    if (interface != Interface::kWindow &&
        interface != Interface::kEventTarget) {
      return nullptr;
    }
    return WindowFromGlobalProxy(isolate, object);
  }
  if (!InterfaceTemplate(isolate, interface)->HasInstance(value)) {
    return nullptr;
  }
  return static_cast<ScriptWrappable*>(
      Object::Unwrap<kWrapperTag, Object::Wrappable>(isolate, object));
}

EventTarget* ReceiverEventTarget(const CallbackInfo& info) {
  Local<Object> receiver = info.This();
  if (IsGlobalProxy(receiver)) {
    return WindowFromGlobalProxy(info.GetIsolate(), receiver);
  }
  return Receiver<EventTarget>(info);
}

void ThrowTypeError(Isolate* isolate, const std::string& message) {
  isolate->ThrowException(
      Exception::TypeError(ToV8StringFromUtf8(isolate, message)));
}

namespace {

int LegacyDOMExceptionCode(std::string_view name) {
  struct Entry {
    const char* name;
    int code;
  };
  static const Entry kCodes[] = {{"IndexSizeError", 1},
                                 {"HierarchyRequestError", 3},
                                 {"WrongDocumentError", 4},
                                 {"InvalidCharacterError", 5},
                                 {"NoModificationAllowedError", 7},
                                 {"NotFoundError", 8},
                                 {"NotSupportedError", 9},
                                 {"InUseAttributeError", 10},
                                 {"InvalidStateError", 11},
                                 {"SyntaxError", 12},
                                 {"InvalidModificationError", 13},
                                 {"NamespaceError", 14},
                                 {"InvalidAccessError", 15},
                                 {"SecurityError", 18},
                                 {"NetworkError", 19},
                                 {"AbortError", 20},
                                 {"URLMismatchError", 21},
                                 {"QuotaExceededError", 22},
                                 {"TimeoutError", 23},
                                 {"InvalidNodeTypeError", 24},
                                 {"DataCloneError", 25}};
  for (const Entry& entry : kCodes) {
    if (name == entry.name) return entry.code;
  }
  return 0;
}

// Turns |error| (an Error object) into a DOMException of the current realm.
void MakeDOMException(Isolate* isolate, Local<Object> error,
                      const std::string& name) {
  Local<Context> context = isolate->GetCurrentContext();
  Local<Function> constructor;
  Local<Value> prototype;
  if (InterfaceTemplate(isolate, Interface::kDOMException)
          ->GetFunction(context)
          .ToLocal(&constructor) &&
      constructor->Get(context, V8Name(isolate, "prototype"))
          .ToLocal(&prototype)) {
    USE(error->SetPrototype(context, prototype));
  }
  USE(error->DefineOwnProperty(context, V8Name(isolate, "name"),
                               ToV8StringFromUtf8(isolate, name), DontEnum));
  USE(error->DefineOwnProperty(
      context, V8Name(isolate, "code"),
      Integer::New(isolate, LegacyDOMExceptionCode(name)), DontEnum));
}

}  // namespace

void ThrowDOMException(Isolate* isolate, const char* name,
                       const std::string& message) {
  Local<Value> error = Exception::Error(ToV8StringFromUtf8(isolate, message));
  MakeDOMException(isolate, error.As<Object>(), name);
  isolate->ThrowException(error);
}

Local<Value> NewDOMException(Isolate* isolate, const char* name,
                             const std::string& message) {
  Local<Value> error = Exception::Error(ToV8StringFromUtf8(isolate, message));
  MakeDOMException(isolate, error.As<Object>(), name);
  return error;
}

bool ThrowIfFailed(Isolate* isolate, const ExceptionState& exception_state) {
  if (!exception_state.failed) return false;
  if (strcmp(exception_state.name, "TypeError") == 0) {
    ThrowTypeError(isolate, exception_state.message);
  } else {
    ThrowDOMException(isolate, exception_state.name, exception_state.message);
  }
  return true;
}

void IllegalConstructor(const CallbackInfo& info) {
  ThrowTypeError(info.GetIsolate(), "Illegal constructor");
}

bool RequireConstructCall(const CallbackInfo& info) {
  if (info.IsConstructCall()) return true;
  ThrowTypeError(info.GetIsolate(),
                 "Please use the 'new' operator, this DOM object constructor "
                 "cannot be called as a function.");
  return false;
}

bool RequireArguments(const CallbackInfo& info, int count) {
  if (info.Length() >= count) return true;
  ThrowTypeError(info.GetIsolate(),
                 std::to_string(count) + " argument" + (count == 1 ? "" : "s") +
                     " required, but only " + std::to_string(info.Length()) +
                     " present.");
  return false;
}

bool ToString(const CallbackInfo& info, Local<Value> value, String* out) {
  Isolate* isolate = info.GetIsolate();
  if (value->IsString()) {
    *out = ToDOMString(isolate, value.As<v8::String>());
    return true;
  }
  Local<v8::String> string;
  if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&string)) {
    return false;
  }
  *out = ToDOMString(isolate, string);
  return true;
}

bool ToAtom(const CallbackInfo& info, Local<Value> value, AtomicString* out) {
  Isolate* isolate = info.GetIsolate();
  if (value->IsString()) {
    *out = ToAtomicString(isolate, value.As<v8::String>());
    return true;
  }
  Local<v8::String> string;
  if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&string)) {
    return false;
  }
  *out = ToAtomicString(isolate, string);
  return true;
}

bool ToStdString(const CallbackInfo& info, Local<Value> value,
                 std::string* out) {
  Isolate* isolate = info.GetIsolate();
  Local<v8::String> string;
  if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&string)) {
    return false;
  }
  *out = ToUtf8(isolate, string);
  return true;
}

bool ToStringNullAsEmpty(const CallbackInfo& info, Local<Value> value,
                         String* out) {
  if (value->IsNull()) {
    *out = String::Empty();
    return true;
  }
  return ToString(info, value, out);
}

bool ToStringOrNull(const CallbackInfo& info, Local<Value> value, String* out) {
  if (value->IsNullOrUndefined()) {
    *out = String();
    return true;
  }
  return ToString(info, value, out);
}

bool ToNumber(const CallbackInfo& info, Local<Value> value, double* out) {
  return value->NumberValue(info.GetIsolate()->GetCurrentContext()).To(out);
}

bool ToInt(const CallbackInfo& info, Local<Value> value, int* out) {
  return value->Int32Value(info.GetIsolate()->GetCurrentContext()).To(out);
}

Node* ToNodeOrThrow(const CallbackInfo& info, Local<Value> value,
                    const char* what) {
  Node* node = To<Node>(info.GetIsolate(), value, Interface::kNode);
  if (!node) {
    ThrowTypeError(info.GetIsolate(),
                   std::string(what) + " is not of type 'Node'.");
  }
  return node;
}

bool GetProperty(Isolate* isolate, Local<Object> object, const char* name,
                 Local<Value>* out) {
  return object->Get(isolate->GetCurrentContext(), V8Name(isolate, name))
      .ToLocal(out);
}

namespace {

#define CALLBACK(name) void name(const CallbackInfo& info)

// --- EventTarget -------------------------------------------------------------

CALLBACK(EventTargetConstructor) {
  if (!RequireConstructCall(info)) return;
  Isolate* isolate = info.GetIsolate();
  EventTarget* target =
      MakeGarbageCollected<EventTarget>(isolate, Interface::kEventTarget);
  Object::Wrap<kWrapperTag>(isolate, info.This(), target);
  target->set_wrapper(isolate, info.This());
}

CALLBACK(AddEventListener) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 2)) return;
  EventTarget* target = ReceiverEventTarget(info);
  AtomicString type;
  if (!ToAtom(info, info[0], &type)) return;
  Local<Value> callback = info[1];
  if (!callback->IsObject()) return;  // null / undefined listener: no-op
  bool capture = false, once = false, passive = false;
  AbortSignal* signal = nullptr;
  if (info.Length() > 2) {
    if (info[2]->IsObject()) {
      Local<Object> options = info[2].As<Object>();
      Local<Value> value;
      if (!GetProperty(isolate, options, "capture", &value)) return;
      capture = value->BooleanValue(isolate);
      if (!GetProperty(isolate, options, "once", &value)) return;
      once = value->BooleanValue(isolate);
      if (!GetProperty(isolate, options, "passive", &value)) return;
      passive = value->BooleanValue(isolate);
      if (!GetProperty(isolate, options, "signal", &value)) return;
      if (!value->IsUndefined()) {
        signal = To<AbortSignal>(isolate, value, Interface::kAbortSignal);
        if (!signal) {
          ThrowTypeError(isolate,
                         "Failed to read the 'signal' property from "
                         "'AddEventListenerOptions': Failed to convert value "
                         "to 'AbortSignal'.");
          return;
        }
        if (signal->aborted()) return;
      }
    } else {
      capture = info[2]->BooleanValue(isolate);
    }
  }
  target->AddEventListener(isolate, type, callback, capture, once, passive);
  if (signal) {
    signal->AddListenerRemoval(isolate, target, type, callback, capture);
  }
}

CALLBACK(RemoveEventListener) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 2)) return;
  EventTarget* target = ReceiverEventTarget(info);
  AtomicString type;
  if (!ToAtom(info, info[0], &type)) return;
  if (!info[1]->IsObject()) return;
  bool capture = false;
  if (info.Length() > 2) {
    if (info[2]->IsObject()) {
      Local<Value> value;
      if (!GetProperty(isolate, info[2].As<Object>(), "capture", &value)) {
        return;
      }
      capture = value->BooleanValue(isolate);
    } else {
      capture = info[2]->BooleanValue(isolate);
    }
  }
  target->RemoveEventListener(isolate, type, info[1], capture);
}

CALLBACK(DispatchEvent) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  EventTarget* target = ReceiverEventTarget(info);
  Event* event = To<Event>(isolate, info[0], Interface::kEvent);
  if (!event) {
    ThrowTypeError(isolate,
                   "Failed to execute 'dispatchEvent' on 'EventTarget': "
                   "parameter 1 is not of type 'Event'.");
    return;
  }
  if (event->dispatching) {
    ThrowDOMException(isolate, "InvalidStateError",
                      "The event is already being dispatched.");
    return;
  }
  event->is_trusted = false;
  info.GetReturnValue().Set(target->DispatchEvent(isolate, event));
}

// --- Event -------------------------------------------------------------------

bool InitEventFromDictionary(const CallbackInfo& info, Event* event,
                             Local<Value> dictionary) {
  Isolate* isolate = info.GetIsolate();
  if (dictionary->IsNullOrUndefined()) return true;
  if (!dictionary->IsObject()) {
    ThrowTypeError(isolate, "The provided value is not of type 'EventInit'.");
    return false;
  }
  Local<Object> init = dictionary.As<Object>();
  Local<Value> value;
  if (!GetProperty(isolate, init, "bubbles", &value)) return false;
  event->bubbles = value->BooleanValue(isolate);
  if (!GetProperty(isolate, init, "cancelable", &value)) return false;
  event->cancelable = value->BooleanValue(isolate);
  if (!GetProperty(isolate, init, "composed", &value)) return false;
  event->composed = value->BooleanValue(isolate);

  const std::vector<EventField>& fields = EventFieldsFor(event->interface());
  for (size_t i = 0; i < fields.size(); ++i) {
    if (!GetProperty(isolate, init, fields[i].name, &value)) return false;
    if (value->IsUndefined()) continue;
    switch (fields[i].kind) {
      case EventField::kNumber:
        if (!ToNumber(info, value, &event->slots[i].number)) return false;
        break;
      case EventField::kBool:
        event->slots[i].number = value->BooleanValue(isolate) ? 1 : 0;
        break;
      case EventField::kString:
        if (value->IsNull() && event->slots[i].string.is_null()) break;
        if (!ToString(info, value, &event->slots[i].string)) return false;
        break;
      case EventField::kAny:
        event->any_slots[i].Reset(isolate, value);
        break;
      case EventField::kTarget:
        event->target_slots[i] =
            To<EventTarget>(isolate, value, Interface::kEventTarget);
        break;
    }
  }
  return true;
}

CALLBACK(EventConstructor) {
  if (!RequireConstructCall(info) || !RequireArguments(info, 1)) return;
  Isolate* isolate = info.GetIsolate();
  Interface interface = static_cast<Interface>(DataInt(info));
  AtomicString type;
  if (!ToAtom(info, info[0], &type)) return;
  Event* event = MakeGarbageCollected<Event>(isolate, isolate, interface, type);
  if (info.Length() > 1 && !InitEventFromDictionary(info, event, info[1])) {
    return;
  }
  Object::Wrap<kWrapperTag>(isolate, info.This(), event);
  event->set_wrapper(isolate, info.This());
}

CALLBACK(EventFieldGetter) {
  Isolate* isolate = info.GetIsolate();
  Event* event = Receiver<Event>(info);
  int index = DataInt(info);
  const EventField& field = EventFieldsFor(event->interface())[index];
  switch (field.kind) {
    case EventField::kNumber:
      info.GetReturnValue().Set(event->slots[index].number);
      return;
    case EventField::kBool:
      info.GetReturnValue().Set(event->slots[index].number != 0);
      return;
    case EventField::kString:
      ReturnOrNull(info, event->slots[index].string);
      return;
    case EventField::kAny:
      if (event->any_slots[index].IsEmpty()) {
        if (strcmp(field.name, "error") == 0) return;  // undefined
        info.GetReturnValue().SetNull();
      } else {
        info.GetReturnValue().Set(event->any_slots[index].Get(isolate));
      }
      return;
    case EventField::kTarget:
      Return(info, event->target_slots[index].Get());
      return;
  }
}

template <bool Event::* Flag>
CALLBACK(EventFlagGetter) {
  info.GetReturnValue().Set(Receiver<Event>(info)->*Flag);
}

CALLBACK(EventInitEvent) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Event* event = Receiver<Event>(info);
  if (event->dispatching) return;
  AtomicString type;
  if (!ToAtom(info, info[0], &type)) return;
  event->set_type(type);
  event->bubbles = info.Length() > 1 && info[1]->BooleanValue(isolate);
  event->cancelable = info.Length() > 2 && info[2]->BooleanValue(isolate);
  event->default_prevented = false;
  event->propagation_stopped = false;
  event->immediate_propagation_stopped = false;
  // initCustomEvent(type, bubbles, cancelable, detail) and
  // initUIEvent(type, bubbles, cancelable, view, detail) share this path.
  int detail_index = event->FieldIndex("detail");
  if (detail_index >= 0) {
    bool is_custom = event->interface() == Interface::kCustomEvent;
    int argument = is_custom ? 3 : 4;
    if (info.Length() > argument) {
      if (is_custom) {
        event->any_slots[detail_index].Reset(isolate, info[argument]);
      } else if (!ToNumber(info, info[argument],
                           &event->slots[detail_index].number)) {
        return;
      }
    }
  }
}

CALLBACK(EventComposedPath) {
  Isolate* isolate = info.GetIsolate();
  Event* event = Receiver<Event>(info);
  LocalVector<Value> elements(isolate);
  if (event->path && event->dispatching) {
    NodeList* path = event->path.Get();
    for (uint32_t i = 0; i < path->length(); ++i) {
      elements.push_back(ToV8(isolate, path->item(i)));
    }
    Node* last = path->length() ? path->item(path->length() - 1) : nullptr;
    if (last && last->IsDocument() &&
        static_cast<Document*>(last)->default_view()) {
      elements.push_back(ToV8(isolate, static_cast<Document*>(last)->window()));
    }
  }
  info.GetReturnValue().Set(
      Array::New(isolate, elements.data(), elements.size()));
}

void InstallEventBindings(Isolate* isolate) {
  SetConstructor(isolate, Interface::kEventTarget, EventTargetConstructor);
  DefineMethod(isolate, Interface::kEventTarget, "addEventListener",
               AddEventListener, 2);
  DefineMethod(isolate, Interface::kEventTarget, "removeEventListener",
               RemoveEventListener, 2);
  DefineMethod(isolate, Interface::kEventTarget, "dispatchEvent", DispatchEvent,
               1);

  const Interface kEvent = Interface::kEvent;
  for (int i = 0; i < kInterfaceCount; ++i) {
    Interface interface = static_cast<Interface>(i);
    if (!InterfaceInheritsFrom(interface, kEvent)) continue;
    SetConstructor(isolate, interface, EventConstructor, 1);
    const std::vector<EventField>& fields = EventFieldsFor(interface);
    Interface parent = InterfaceParent(interface);
    size_t inherited =
        parent == Interface::kNone ? 0 : EventFieldsFor(parent).size();
    for (size_t f = inherited; f < fields.size(); ++f) {
      DefineAccessor(isolate, interface, fields[f].name, EventFieldGetter,
                     nullptr, Integer::New(isolate, static_cast<int>(f)));
    }
  }

  DefineAccessor(isolate, kEvent, "type", [](const CallbackInfo& info) {
    Return(info, Receiver<Event>(info)->type());
  });
  DefineAccessor(isolate, kEvent, "target", [](const CallbackInfo& info) {
    Return(info, Receiver<Event>(info)->target.Get());
  });
  DefineAccessor(isolate, kEvent, "srcElement", [](const CallbackInfo& info) {
    Return(info, Receiver<Event>(info)->target.Get());
  });
  DefineAccessor(isolate, kEvent, "currentTarget",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Event>(info)->current_target.Get());
                 });
  DefineAccessor(isolate, kEvent, "eventPhase", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(static_cast<int>(Receiver<Event>(info)->phase));
  });
  DefineAccessor(isolate, kEvent, "bubbles", EventFlagGetter<&Event::bubbles>);
  DefineAccessor(isolate, kEvent, "cancelable",
                 EventFlagGetter<&Event::cancelable>);
  DefineAccessor(isolate, kEvent, "composed",
                 EventFlagGetter<&Event::composed>);
  DefineAccessor(isolate, kEvent, "defaultPrevented",
                 EventFlagGetter<&Event::default_prevented>);
  DefineAccessor(isolate, kEvent, "isTrusted",
                 EventFlagGetter<&Event::is_trusted>);
  DefineAccessor(isolate, kEvent, "timeStamp", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(Receiver<Event>(info)->time_stamp);
  });
  DefineAccessor(
      isolate, kEvent, "returnValue",
      [](const CallbackInfo& info) {
        info.GetReturnValue().Set(!Receiver<Event>(info)->default_prevented);
      },
      [](const CallbackInfo& info) {
        Event* event = Receiver<Event>(info);
        if (!info[0]->BooleanValue(info.GetIsolate()) && event->cancelable &&
            !event->in_passive_listener) {
          event->default_prevented = true;
        }
      });
  DefineAccessor(
      isolate, kEvent, "cancelBubble",
      [](const CallbackInfo& info) {
        info.GetReturnValue().Set(Receiver<Event>(info)->propagation_stopped);
      },
      [](const CallbackInfo& info) {
        if (info[0]->BooleanValue(info.GetIsolate())) {
          Receiver<Event>(info)->propagation_stopped = true;
        }
      });
  DefineMethod(isolate, kEvent, "stopPropagation",
               [](const CallbackInfo& info) {
                 Receiver<Event>(info)->propagation_stopped = true;
               });
  DefineMethod(isolate, kEvent, "stopImmediatePropagation",
               [](const CallbackInfo& info) {
                 Event* event = Receiver<Event>(info);
                 event->propagation_stopped = true;
                 event->immediate_propagation_stopped = true;
               });
  DefineMethod(isolate, kEvent, "preventDefault", [](const CallbackInfo& info) {
    Event* event = Receiver<Event>(info);
    if (event->cancelable && !event->in_passive_listener) {
      event->default_prevented = true;
    }
  });
  DefineMethod(isolate, kEvent, "composedPath", EventComposedPath);
  DefineMethod(isolate, kEvent, "initEvent", EventInitEvent, 1);
  DefineMethod(isolate, Interface::kCustomEvent, "initCustomEvent",
               EventInitEvent, 1);
  DefineMethod(isolate, Interface::kUIEvent, "initUIEvent", EventInitEvent, 1);
  DefineConstant(isolate, kEvent, "NONE", 0);
  DefineConstant(isolate, kEvent, "CAPTURING_PHASE", 1);
  DefineConstant(isolate, kEvent, "AT_TARGET", 2);
  DefineConstant(isolate, kEvent, "BUBBLING_PHASE", 3);

  // Legacy and derived members.
  DefineAccessor(isolate, Interface::kKeyboardEvent, "which",
                 [](const CallbackInfo& info) {
                   Event* event = Receiver<Event>(info);
                   double which = event->number("which");
                   if (which == 0) which = event->number("keyCode");
                   if (which == 0) which = event->number("charCode");
                   info.GetReturnValue().Set(which);
                 });
  DefineAccessor(
      isolate, Interface::kMouseEvent, "which", [](const CallbackInfo& info) {
        info.GetReturnValue().Set(Receiver<Event>(info)->number("button") + 1);
      });
  // Without layout or scrolling, page, offset and layer coordinates all
  // coincide with client coordinates.
  static const char* const kXAliases[] = {"x", "pageX", "offsetX", "layerX"};
  static const char* const kYAliases[] = {"y", "pageY", "offsetY", "layerY"};
  for (const char* name : kXAliases) {
    DefineAccessor(
        isolate, Interface::kMouseEvent, name, [](const CallbackInfo& info) {
          info.GetReturnValue().Set(Receiver<Event>(info)->number("clientX"));
        });
  }
  for (const char* name : kYAliases) {
    DefineAccessor(
        isolate, Interface::kMouseEvent, name, [](const CallbackInfo& info) {
          info.GetReturnValue().Set(Receiver<Event>(info)->number("clientY"));
        });
  }
  auto get_modifier_state = [](const CallbackInfo& info) {
    Event* event = Receiver<Event>(info);
    std::string key;
    if (info.Length() < 1 || !ToStdString(info, info[0], &key)) return;
    const char* field = key == "Control" ? "ctrlKey"
                        : key == "Shift" ? "shiftKey"
                        : key == "Alt"   ? "altKey"
                        : key == "Meta"  ? "metaKey"
                                         : nullptr;
    info.GetReturnValue().Set(field && event->number(field) != 0);
  };
  DefineMethod(isolate, Interface::kMouseEvent, "getModifierState",
               get_modifier_state, 1);
  DefineMethod(isolate, Interface::kKeyboardEvent, "getModifierState",
               get_modifier_state, 1);
  DefineConstant(isolate, Interface::kWheelEvent, "DOM_DELTA_PIXEL", 0);
  DefineConstant(isolate, Interface::kWheelEvent, "DOM_DELTA_LINE", 1);
  DefineConstant(isolate, Interface::kWheelEvent, "DOM_DELTA_PAGE", 2);
}

// --- Node --------------------------------------------------------------------

ContainerNode* AsContainer(Node* node) {
  return node->IsContainer() ? static_cast<ContainerNode*>(node) : nullptr;
}

template <Node* (Node::*Getter)() const>
CALLBACK(NodeLinkGetter) {
  Return(info, (Receiver<Node>(info)->*Getter)());
}

CALLBACK(NodeChildNodes) {
  Isolate* isolate = info.GetIsolate();
  Node* node = Receiver<Node>(info);
  ContainerNode* container = AsContainer(node);
  if (!container) {
    MOCK_DOM_LOCAL_NODES
    std::vector<Node*> none;
    Return(info, NodeList::CreateStatic(isolate, none));
    return;
  }
  if (!container->child_nodes_list) {
    container->child_nodes_list = MakeGarbageCollected<NodeList>(
        isolate, Interface::kNodeList, NodeList::kChildNodes, node,
        AtomicString());
  }
  Return(info, container->child_nodes_list.Get());
}

CALLBACK(NodeInsertBefore) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 2)) return;
  Node* parent = Receiver<Node>(info);
  Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!node) return;
  Node* child = nullptr;
  if (!info[1]->IsNullOrUndefined()) {
    child = ToNodeOrThrow(info, info[1], "parameter 2");
    if (!child) return;
  }
  ContainerNode* container = AsContainer(parent);
  if (!container) {
    ThrowDOMException(isolate, "HierarchyRequestError",
                      "This node type does not support this method.");
    return;
  }
  ExceptionState exception_state;
  container->InsertBefore(isolate, node, child, &exception_state);
  if (ThrowIfFailed(isolate, exception_state)) return;
  info.GetReturnValue().Set(info[0]);
}

CALLBACK(NodeAppendChild) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Node* parent = Receiver<Node>(info);
  Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!node) return;
  ContainerNode* container = AsContainer(parent);
  if (!container) {
    ThrowDOMException(isolate, "HierarchyRequestError",
                      "This node type does not support this method.");
    return;
  }
  ExceptionState exception_state;
  container->AppendChild(isolate, node, &exception_state);
  if (ThrowIfFailed(isolate, exception_state)) return;
  info.GetReturnValue().Set(info[0]);
}

CALLBACK(NodeRemoveChild) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Node* parent = Receiver<Node>(info);
  Node* child = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!child) return;
  ContainerNode* container = AsContainer(parent);
  ExceptionState exception_state;
  if (!container) {
    exception_state.Throw(
        "NotFoundError", "The node to be removed is not a child of this node.");
  } else {
    container->RemoveChild(isolate, child, &exception_state);
  }
  if (ThrowIfFailed(isolate, exception_state)) return;
  info.GetReturnValue().Set(info[0]);
}

CALLBACK(NodeReplaceChild) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 2)) return;
  Node* parent = Receiver<Node>(info);
  Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!node) return;
  Node* child = ToNodeOrThrow(info, info[1], "parameter 2");
  if (!child) return;
  ContainerNode* container = AsContainer(parent);
  ExceptionState exception_state;
  if (!container) {
    exception_state.Throw(
        "NotFoundError",
        "The node to be replaced is not a child of this node.");
  } else {
    container->ReplaceChild(isolate, node, child, &exception_state);
  }
  if (ThrowIfFailed(isolate, exception_state)) return;
  info.GetReturnValue().Set(info[1]);
}

CALLBACK(NodeCloneNode) {
  Isolate* isolate = info.GetIsolate();
  Node* node = Receiver<Node>(info);
  bool deep = info.Length() > 0 && info[0]->BooleanValue(isolate);
  Document* document =
      node->IsDocument() ? static_cast<Document*>(node) : node->document();
  Node* clone = node->Clone(isolate, document, deep);
  if (!clone) {
    ThrowDOMException(isolate, "NotSupportedError",
                      "mock-dom: cloning this node type is not supported.");
    return;
  }
  Return(info, clone);
}

CALLBACK(NodeGetRootNode) {
  Isolate* isolate = info.GetIsolate();
  Node* node = Receiver<Node>(info);
  bool composed = false;
  if (info.Length() > 0 && info[0]->IsObject()) {
    Local<Value> value;
    if (!GetProperty(isolate, info[0].As<Object>(), "composed", &value)) {
      return;
    }
    composed = value->BooleanValue(isolate);
  }
  Node* root = node->Root();
  while (composed && root->IsShadowRoot()) {
    root = static_cast<ShadowRoot*>(root)->host()->Root();
  }
  Return(info, root);
}

// Converts the arguments of append()/prepend()/before()/after()/
// replaceWith()/replaceChildren() into a single node.
Node* NodesIntoNode(const CallbackInfo& info, Document* document) {
  Isolate* isolate = info.GetIsolate();
  auto convert = [&](Local<Value> value) -> Node* {
    if (Node* node = To<Node>(isolate, value, Interface::kNode)) return node;
    String text;
    if (!ToString(info, value, &text)) return nullptr;
    return document->CreateTextNode(isolate, text);
  };
  if (info.Length() == 1) return convert(info[0]);
  DocumentFragment* fragment = document->CreateDocumentFragment(isolate);
  for (int i = 0; i < info.Length(); ++i) {
    Node* node = convert(info[i]);
    if (!node) return nullptr;
    ExceptionState exception_state;
    fragment->AppendChild(isolate, node, &exception_state);
    if (ThrowIfFailed(isolate, exception_state)) return nullptr;
  }
  return fragment;
}

Document* DocumentOf(Node* node) {
  return node->IsDocument() ? static_cast<Document*>(node) : node->document();
}

enum class ParentNodeOp { kAppend, kPrepend, kReplaceChildren };

template <ParentNodeOp op>
CALLBACK(ParentNodeMutation) {
  Isolate* isolate = info.GetIsolate();
  ContainerNode* parent = static_cast<ContainerNode*>(Receiver<Node>(info));
  CEReactionsScope reactions(isolate);
  if (op == ParentNodeOp::kReplaceChildren) parent->RemoveChildren(isolate);
  if (info.Length() == 0) return;
  Node* node = NodesIntoNode(info, DocumentOf(parent));
  if (!node) return;
  ExceptionState exception_state;
  parent->InsertBefore(isolate, node,
                       op == ParentNodeOp::kPrepend ? parent->first() : nullptr,
                       &exception_state);
  ThrowIfFailed(isolate, exception_state);
}

enum class ChildNodeOp { kBefore, kAfter, kReplaceWith };

template <ChildNodeOp op>
CALLBACK(ChildNodeMutation) {
  Isolate* isolate = info.GetIsolate();
  Node* self = Receiver<Node>(info);
  ContainerNode* parent = self->parent();
  if (!parent) return;
  CEReactionsScope reactions(isolate);
  // The reference node must not be one of the nodes being inserted.
  auto is_argument = [&](Node* candidate) {
    for (int i = 0; i < info.Length(); ++i) {
      if (To<Node>(isolate, info[i], Interface::kNode) == candidate) {
        return true;
      }
    }
    return false;
  };
  Node* reference;
  if (op == ChildNodeOp::kBefore) {
    Node* previous = self->previous_sibling();
    while (previous && is_argument(previous)) {
      previous = previous->previous_sibling();
    }
    reference = previous ? previous->next_sibling() : parent->first();
    if (info.Length() == 0) return;
  } else {
    reference = self->next_sibling();
    while (reference && is_argument(reference)) {
      reference = reference->next_sibling();
    }
  }
  Node* node = info.Length() ? NodesIntoNode(info, DocumentOf(self)) : nullptr;
  if (info.Length() && !node) return;
  ExceptionState exception_state;
  if (op == ChildNodeOp::kReplaceWith && self->parent() == parent &&
      !(node && node == self)) {
    parent->RemoveChild(isolate, self, &exception_state);
  }
  if (node) parent->InsertBefore(isolate, node, reference, &exception_state);
  ThrowIfFailed(isolate, exception_state);
}

CALLBACK(ParentNodeChildren) {
  Isolate* isolate = info.GetIsolate();
  ContainerNode* container = static_cast<ContainerNode*>(Receiver<Node>(info));
  if (!container->children_list) {
    container->children_list = MakeGarbageCollected<NodeList>(
        isolate, Interface::kHTMLCollection, NodeList::kChildren, container,
        AtomicString());
  }
  Return(info, container->children_list.Get());
}

CALLBACK(ParentNodeQuerySelector) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  ContainerNode* container = static_cast<ContainerNode*>(Receiver<Node>(info));
  AtomicString selectors;
  if (!ToAtom(info, info[0], &selectors)) return;
  ExceptionState exception_state;
  Element* result =
      container->QuerySelector(isolate, selectors, &exception_state);
  if (ThrowIfFailed(isolate, exception_state)) return;
  Return(info, result);
}

CALLBACK(ParentNodeQuerySelectorAll) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  ContainerNode* container = static_cast<ContainerNode*>(Receiver<Node>(info));
  AtomicString selectors;
  if (!ToAtom(info, info[0], &selectors)) return;
  ExceptionState exception_state;
  MOCK_DOM_LOCAL_NODES
  std::vector<Element*> results;
  container->QuerySelectorAll(isolate, selectors, &results, &exception_state);
  if (ThrowIfFailed(isolate, exception_state)) return;
  Return(info, NodeList::CreateStatic(isolate, results));
}

void InstallParentNodeMixin(Isolate* isolate, Interface interface) {
  DefineAccessor(isolate, interface, "children", ParentNodeChildren);
  DefineAccessor(
      isolate, interface, "firstElementChild", [](const CallbackInfo& info) {
        Return(info, static_cast<ContainerNode*>(Receiver<Node>(info))
                         ->first_element_child());
      });
  DefineAccessor(
      isolate, interface, "lastElementChild", [](const CallbackInfo& info) {
        Return(info, static_cast<ContainerNode*>(Receiver<Node>(info))
                         ->last_element_child());
      });
  DefineAccessor(isolate, interface, "childElementCount",
                 [](const CallbackInfo& info) {
                   info.GetReturnValue().Set(
                       static_cast<ContainerNode*>(Receiver<Node>(info))
                           ->CountElementChildren());
                 });
  DefineMethod(isolate, interface, "append",
               ParentNodeMutation<ParentNodeOp::kAppend>);
  DefineMethod(isolate, interface, "prepend",
               ParentNodeMutation<ParentNodeOp::kPrepend>);
  DefineMethod(isolate, interface, "replaceChildren",
               ParentNodeMutation<ParentNodeOp::kReplaceChildren>);
  DefineMethod(isolate, interface, "querySelector", ParentNodeQuerySelector, 1);
  DefineMethod(isolate, interface, "querySelectorAll",
               ParentNodeQuerySelectorAll, 1);
}

void InstallChildNodeMixin(Isolate* isolate, Interface interface) {
  DefineMethod(isolate, interface, "before",
               ChildNodeMutation<ChildNodeOp::kBefore>);
  DefineMethod(isolate, interface, "after",
               ChildNodeMutation<ChildNodeOp::kAfter>);
  DefineMethod(isolate, interface, "replaceWith",
               ChildNodeMutation<ChildNodeOp::kReplaceWith>);
  DefineMethod(isolate, interface, "remove", [](const CallbackInfo& info) {
    Receiver<Node>(info)->Remove(info.GetIsolate());
  });
  if (interface == Interface::kDocumentType) return;
  DefineAccessor(isolate, interface, "previousElementSibling",
                 [](const CallbackInfo& info) {
                   Return(info,
                          Receiver<Node>(info)->previous_element_sibling());
                 });
  DefineAccessor(isolate, interface, "nextElementSibling",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Node>(info)->next_element_sibling());
                 });
}

void InstallNodeBindings(Isolate* isolate) {
  const Interface kNode = Interface::kNode;
  DefineAccessor(isolate, kNode, "nodeType", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        static_cast<int>(Receiver<Node>(info)->node_type()));
  });
  DefineAccessor(isolate, kNode, "nodeName", [](const CallbackInfo& info) {
    Return(info, Receiver<Node>(info)->NodeName());
  });
  DefineAccessor(isolate, kNode, "isConnected", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(Receiver<Node>(info)->is_connected());
  });
  DefineAccessor(isolate, kNode, "ownerDocument", [](const CallbackInfo& info) {
    Return(info, Receiver<Node>(info)->document());
  });
  DefineAccessor(isolate, kNode, "baseURI", [](const CallbackInfo& info) {
    Window* window = Receiver<Node>(info)->WrapperWindow();
    info.GetReturnValue().Set(ToV8StringFromUtf8(
        info.GetIsolate(), window->location()->url().Href()));
  });
  DefineAccessor(isolate, kNode, "parentNode", [](const CallbackInfo& info) {
    Return(info, Receiver<Node>(info)->parent());
  });
  DefineAccessor(isolate, kNode, "parentElement", [](const CallbackInfo& info) {
    Return(info, Receiver<Node>(info)->parent_element());
  });
  DefineAccessor(isolate, kNode, "childNodes", NodeChildNodes);
  DefineAccessor(isolate, kNode, "firstChild",
                 NodeLinkGetter<&Node::first_child>);
  DefineAccessor(isolate, kNode, "lastChild",
                 NodeLinkGetter<&Node::last_child>);
  DefineAccessor(isolate, kNode, "previousSibling",
                 NodeLinkGetter<&Node::previous_sibling>);
  DefineAccessor(isolate, kNode, "nextSibling",
                 NodeLinkGetter<&Node::next_sibling>);
  DefineAccessor(
      isolate, kNode, "nodeValue",
      [](const CallbackInfo& info) {
        ReturnOrNull(info, Receiver<Node>(info)->NodeValue());
      },
      [](const CallbackInfo& info) {
        Node* node = Receiver<Node>(info);
        if (!node->IsCharacterData()) return;
        String value;
        if (!ToStringNullAsEmpty(info, info[0], &value)) return;
        static_cast<CharacterData*>(node)->SetData(info.GetIsolate(), value);
      });
  DefineAccessor(
      isolate, kNode, "textContent",
      [](const CallbackInfo& info) {
        ReturnOrNull(info, Receiver<Node>(info)->TextContent());
      },
      [](const CallbackInfo& info) {
        String value;
        if (!ToStringNullAsEmpty(info, info[0], &value)) return;
        Receiver<Node>(info)->SetTextContent(info.GetIsolate(), value);
      });
  DefineMethod(isolate, kNode, "getRootNode", NodeGetRootNode);
  DefineMethod(isolate, kNode, "hasChildNodes", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(Receiver<Node>(info)->first_child() != nullptr);
  });
  DefineMethod(isolate, kNode, "normalize", [](const CallbackInfo& info) {
    Receiver<Node>(info)->Normalize(info.GetIsolate());
  });
  DefineMethod(isolate, kNode, "cloneNode", NodeCloneNode);
  DefineMethod(
      isolate, kNode, "isEqualNode",
      [](const CallbackInfo& info) {
        Node* other = info.Length() ? To<Node>(info.GetIsolate(), info[0],
                                               Interface::kNode)
                                    : nullptr;
        info.GetReturnValue().Set(Receiver<Node>(info)->IsEqualNode(other));
      },
      1);
  DefineMethod(
      isolate, kNode, "isSameNode",
      [](const CallbackInfo& info) {
        Node* other = info.Length() ? To<Node>(info.GetIsolate(), info[0],
                                               Interface::kNode)
                                    : nullptr;
        info.GetReturnValue().Set(Receiver<Node>(info) == other);
      },
      1);
  DefineMethod(
      isolate, kNode, "compareDocumentPosition",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        Node* other = ToNodeOrThrow(info, info[0], "parameter 1");
        if (!other) return;
        info.GetReturnValue().Set(static_cast<int>(
            Receiver<Node>(info)->CompareDocumentPosition(other)));
      },
      1);
  DefineMethod(
      isolate, kNode, "contains",
      [](const CallbackInfo& info) {
        Node* other = info.Length() ? To<Node>(info.GetIsolate(), info[0],
                                               Interface::kNode)
                                    : nullptr;
        info.GetReturnValue().Set(other &&
                                  Receiver<Node>(info)->Contains(other));
      },
      1);
  DefineMethod(isolate, kNode, "insertBefore", NodeInsertBefore, 2);
  DefineMethod(isolate, kNode, "appendChild", NodeAppendChild, 1);
  DefineMethod(isolate, kNode, "replaceChild", NodeReplaceChild, 2);
  DefineMethod(isolate, kNode, "removeChild", NodeRemoveChild, 1);

  static const struct {
    const char* name;
    int value;
  } kConstants[] = {
      {"ELEMENT_NODE", 1},
      {"ATTRIBUTE_NODE", 2},
      {"TEXT_NODE", 3},
      {"CDATA_SECTION_NODE", 4},
      {"PROCESSING_INSTRUCTION_NODE", 7},
      {"COMMENT_NODE", 8},
      {"DOCUMENT_NODE", 9},
      {"DOCUMENT_TYPE_NODE", 10},
      {"DOCUMENT_FRAGMENT_NODE", 11},
      {"DOCUMENT_POSITION_DISCONNECTED", 1},
      {"DOCUMENT_POSITION_PRECEDING", 2},
      {"DOCUMENT_POSITION_FOLLOWING", 4},
      {"DOCUMENT_POSITION_CONTAINS", 8},
      {"DOCUMENT_POSITION_CONTAINED_BY", 16},
      {"DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC", 32},
  };
  for (const auto& constant : kConstants) {
    DefineConstant(isolate, kNode, constant.name, constant.value);
  }

  InstallParentNodeMixin(isolate, Interface::kDocument);
  InstallParentNodeMixin(isolate, Interface::kDocumentFragment);
  InstallParentNodeMixin(isolate, Interface::kElement);
  InstallChildNodeMixin(isolate, Interface::kElement);
  InstallChildNodeMixin(isolate, Interface::kCharacterData);
  InstallChildNodeMixin(isolate, Interface::kDocumentType);
}

// --- CharacterData, Text, Comment, DocumentType ------------------------------

// Offsets into character data are UTF-16 code units, like in JS.
std::u16string DataAsUtf16(const String& data) {
  std::u16string out;
  if (data.is_null()) return out;
  StringImpl* impl = data.impl();
  if (impl->is_8bit()) {
    for (char c : impl->view8()) out.push_back(static_cast<unsigned char>(c));
  } else {
    out = std::u16string(impl->view16());
  }
  return out;
}

String MakeString(const std::u16string& text) {
  bool latin1 = true;
  for (char16_t c : text) {
    if (c > 0xFF) {
      latin1 = false;
      break;
    }
  }
  if (!latin1) return String(StringImpl::Create16(text));
  std::string narrow(text.size(), '\0');
  for (size_t i = 0; i < text.size(); ++i) {
    narrow[i] = static_cast<char>(text[i]);
  }
  return String(StringImpl::Create8(narrow));
}

// replaceData(offset, count, data) and the methods defined in terms of it.
bool ReplaceData(const CallbackInfo& info, CharacterData* node, double offset,
                 double count, const String& replacement) {
  Isolate* isolate = info.GetIsolate();
  std::u16string data = DataAsUtf16(node->data());
  if (offset < 0 || offset > static_cast<double>(data.size())) {
    ThrowDOMException(isolate, "IndexSizeError",
                      "The offset is larger than the data's length.");
    return false;
  }
  size_t start = static_cast<size_t>(offset);
  size_t length = count < 0 ? data.size() : static_cast<size_t>(count);
  if (start + length > data.size()) length = data.size() - start;
  data.replace(start, length, DataAsUtf16(replacement));
  node->SetData(isolate, MakeString(data));
  return true;
}

template <Interface interface>
CALLBACK(CharacterDataConstructor) {
  if (!RequireConstructCall(info)) return;
  Isolate* isolate = info.GetIsolate();
  String data = String::Empty();
  if (info.Length() > 0 && !info[0]->IsUndefined() &&
      !ToString(info, info[0], &data)) {
    return;
  }
  Document* document = Window::Current(isolate)->document();
  Node* node = interface == Interface::kText
                   ? document->CreateTextNode(isolate, data)
                   : document->CreateComment(isolate, data);
  Object::Wrap<kWrapperTag>(isolate, info.This(), node);
  node->set_wrapper(isolate, info.This());
}

void InstallCharacterDataBindings(Isolate* isolate) {
  const Interface kCharacterData = Interface::kCharacterData;
  SetConstructor(isolate, Interface::kText,
                 CharacterDataConstructor<Interface::kText>);
  SetConstructor(isolate, Interface::kComment,
                 CharacterDataConstructor<Interface::kComment>);
  DefineAccessor(
      isolate, kCharacterData, "data",
      [](const CallbackInfo& info) {
        Return(info, Receiver<CharacterData>(info)->data());
      },
      [](const CallbackInfo& info) {
        String value;
        if (!ToStringNullAsEmpty(info, info[0], &value)) return;
        Receiver<CharacterData>(info)->SetData(info.GetIsolate(), value);
      });
  DefineAccessor(isolate, kCharacterData, "length",
                 [](const CallbackInfo& info) {
                   info.GetReturnValue().Set(
                       Receiver<CharacterData>(info)->data().length());
                 });
  DefineMethod(
      isolate, kCharacterData, "substringData",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 2)) return;
        double offset, count;
        if (!ToNumber(info, info[0], &offset) ||
            !ToNumber(info, info[1], &count)) {
          return;
        }
        std::u16string data =
            DataAsUtf16(Receiver<CharacterData>(info)->data());
        if (offset < 0 || offset > static_cast<double>(data.size())) {
          ThrowDOMException(info.GetIsolate(), "IndexSizeError",
                            "The offset is larger than the data's length.");
          return;
        }
        Return(info, MakeString(data.substr(static_cast<size_t>(offset),
                                            static_cast<size_t>(count))));
      },
      2);
  DefineMethod(
      isolate, kCharacterData, "appendData",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        String value;
        if (!ToString(info, info[0], &value)) return;
        CharacterData* node = Receiver<CharacterData>(info);
        ReplaceData(info, node, node->data().length(), 0, value);
      },
      1);
  DefineMethod(
      isolate, kCharacterData, "insertData",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 2)) return;
        double offset;
        String value;
        if (!ToNumber(info, info[0], &offset) ||
            !ToString(info, info[1], &value)) {
          return;
        }
        ReplaceData(info, Receiver<CharacterData>(info), offset, 0, value);
      },
      2);
  DefineMethod(
      isolate, kCharacterData, "deleteData",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 2)) return;
        double offset, count;
        if (!ToNumber(info, info[0], &offset) ||
            !ToNumber(info, info[1], &count)) {
          return;
        }
        ReplaceData(info, Receiver<CharacterData>(info), offset, count,
                    String::Empty());
      },
      2);
  DefineMethod(
      isolate, kCharacterData, "replaceData",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 3)) return;
        double offset, count;
        String value;
        if (!ToNumber(info, info[0], &offset) ||
            !ToNumber(info, info[1], &count) ||
            !ToString(info, info[2], &value)) {
          return;
        }
        ReplaceData(info, Receiver<CharacterData>(info), offset, count, value);
      },
      3);

  DefineAccessor(
      isolate, Interface::kText, "wholeText", [](const CallbackInfo& info) {
        Node* node = Receiver<Node>(info);
        Node* first = node;
        while (first->previous_sibling() &&
               first->previous_sibling()->IsText()) {
          first = first->previous_sibling();
        }
        std::string text;
        for (Node* n = first; n && n->IsText(); n = n->next_sibling()) {
          text += static_cast<CharacterData*>(n)->data().ToUtf8();
        }
        Return(info, String::FromUtf8(text));
      });
  DefineMethod(
      isolate, Interface::kText, "splitText",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 1)) return;
        double offset;
        if (!ToNumber(info, info[0], &offset)) return;
        CharacterData* node = Receiver<CharacterData>(info);
        std::u16string data = DataAsUtf16(node->data());
        if (offset < 0 || offset > static_cast<double>(data.size())) {
          ThrowDOMException(isolate, "IndexSizeError",
                            "The offset is larger than the Text node's "
                            "length.");
          return;
        }
        size_t split = static_cast<size_t>(offset);
        Node* tail = node->document()->CreateTextNode(
            isolate, MakeString(data.substr(split)));
        if (ContainerNode* parent = node->parent()) {
          ExceptionState exception_state;
          parent->InsertBefore(isolate, tail, node->next_sibling(),
                               &exception_state);
        }
        node->SetData(isolate, MakeString(data.substr(0, split)));
        Return(info, tail);
      },
      1);

  DefineAccessor(isolate, Interface::kDocumentType, "name",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<DocumentType>(info)->name);
                 });
  DefineAccessor(
      isolate, Interface::kDocumentType, "publicId",
      [](const CallbackInfo& info) { Return(info, String::Empty()); });
  DefineAccessor(
      isolate, Interface::kDocumentType, "systemId",
      [](const CallbackInfo& info) { Return(info, String::Empty()); });
}

// --- Collections -------------------------------------------------------------

Intercepted NodeListIndexedGetter(uint32_t index,
                                  const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  NodeList* list = static_cast<NodeList*>(static_cast<ScriptWrappable*>(
      Object::Unwrap<kWrapperTag, Object::Wrappable>(isolate, info.Holder())));
  Node* node = list->item(index);
  if (!node) return Intercepted::kNo;
  info.GetReturnValue().Set(ToV8(isolate, node));
  return Intercepted::kYes;
}

void NodeListEnumerator(const PropertyCallbackInfo<Array>& info) {
  Isolate* isolate = info.GetIsolate();
  NodeList* list = static_cast<NodeList*>(static_cast<ScriptWrappable*>(
      Object::Unwrap<kWrapperTag, Object::Wrappable>(isolate, info.Holder())));
  uint32_t length = list->length();
  Local<Array> indices = Array::New(isolate, static_cast<int>(length));
  Local<Context> context = isolate->GetCurrentContext();
  for (uint32_t i = 0; i < length; ++i) {
    USE(indices->Set(context, i, Integer::NewFromUnsigned(isolate, i)));
  }
  info.GetReturnValue().Set(indices);
}

uint32_t NodeListIndexOf(Local<Value> value, uint32_t start_index,
                         uint32_t end_index, uint32_t* out_length,
                         const PropertyCallbackInfo<void>& info) {
  Isolate* isolate = info.GetIsolate();
  NodeList* list = static_cast<NodeList*>(static_cast<ScriptWrappable*>(
      Object::Unwrap<kWrapperTag, Object::Wrappable>(isolate, info.Holder())));
  return list->IndexOf(To<Node>(isolate, value, Interface::kNode), start_index,
                       end_index, out_length);
}

void NodeListIterableToList(const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  NodeList* list = static_cast<NodeList*>(static_cast<ScriptWrappable*>(
      Object::Unwrap<kWrapperTag, Object::Wrappable>(isolate, info.Holder())));
  uint32_t length = list->length();
  LocalVector<Value> elements(isolate);
  elements.reserve(length);
  for (uint32_t i = 0; i < length; ++i) {
    elements.push_back(ToV8(isolate, list->item(i)));
  }
  info.GetReturnValue().Set(
      Array::New(isolate, elements.data(), elements.size()));
}

void InstallCollectionBindings(Isolate* isolate) {
  for (Interface interface : {Interface::kNodeList, Interface::kHTMLCollection,
                              Interface::kHTMLAllCollection}) {
    bool is_node_list = interface == Interface::kNodeList;
    InterfaceTemplate(isolate, interface)
        ->InstanceTemplate()
        ->SetHandler(IndexedPropertyHandlerConfiguration(
            NodeListIndexedGetter, nullptr, nullptr, nullptr,
            NodeListEnumerator, nullptr, nullptr,
            is_node_list ? NodeListIndexOf : nullptr,
            is_node_list ? NodeListIterableToList : nullptr));
    DefineAccessor(isolate, interface, "length", [](const CallbackInfo& info) {
      info.GetReturnValue().Set(Receiver<NodeList>(info)->length());
    });
    DefineMethod(
        isolate, interface, "item",
        [](const CallbackInfo& info) {
          double index = 0;
          if (info.Length() > 0 && !ToNumber(info, info[0], &index)) return;
          Node* node =
              index >= 0
                  ? Receiver<NodeList>(info)->item(static_cast<uint32_t>(index))
                  : nullptr;
          Return(info, node);
        },
        1);
  }
  // Web IDL value iterator on NodeList, matching Blink: installed through the
  // template so [Symbol.iterator] stays a constant Array.prototype.values
  // intrinsic for V8's fast IterableToList path.
  Local<ObjectTemplate> node_list_proto =
      InterfaceTemplate(isolate, Interface::kNodeList)->PrototypeTemplate();
  node_list_proto->SetIntrinsicDataProperty(Symbol::GetIterator(isolate),
                                            v8::kArrayProto_values, DontEnum);
  node_list_proto->SetIntrinsicDataProperty(V8Name(isolate, "entries"),
                                            v8::kArrayProto_entries);
  node_list_proto->SetIntrinsicDataProperty(V8Name(isolate, "keys"),
                                            v8::kArrayProto_keys);
  node_list_proto->SetIntrinsicDataProperty(V8Name(isolate, "values"),
                                            v8::kArrayProto_values);
  node_list_proto->SetIntrinsicDataProperty(V8Name(isolate, "forEach"),
                                            v8::kArrayProto_forEach);
  static constexpr FunctionCallback kNamedItem = [](const CallbackInfo& info) {
    if (!RequireArguments(info, 1)) return;
    AtomicString name;
    if (!ToAtom(info, info[0], &name)) return;
    static const AtomicString* name_atom = new AtomicString("name");
    NodeList* list = Receiver<NodeList>(info);
    for (uint32_t i = 0; i < list->length(); ++i) {
      Element* element = static_cast<Element*>(list->item(i));
      if (element->id() == name || element->GetAttribute(*name_atom) == name) {
        Return(info, element);
        return;
      }
    }
    info.GetReturnValue().SetNull();
  };
  for (Interface interface :
       {Interface::kHTMLCollection, Interface::kHTMLAllCollection}) {
    DefineMethod(isolate, interface, "namedItem", kNamedItem, 1);
  }
  // document.all is the one undetectable object of the web platform: typeof
  // gives "undefined" and it compares equal to null. Creating it is what makes
  // a browser drop V8's NoUndetectableObjects protector, so pages that touch
  // document.all run the rest of their code without that protector. V8 drops
  // it when the interface is first instantiated.
  Local<ObjectTemplate> all_collection =
      InterfaceTemplate(isolate, Interface::kHTMLAllCollection)
          ->InstanceTemplate();
  all_collection->MarkAsUndetectable();
  // V8 requires undetectable objects to be callable; document.all(name) is the
  // legacy way to call namedItem.
  all_collection->SetCallAsFunctionHandler(kNamedItem);
  // The interface must not be instantiated before a page asks for
  // document.all, so its iterator is installed through the template rather
  // than on the prototype object like for the other collections.
  InterfaceTemplate(isolate, Interface::kHTMLAllCollection)
      ->PrototypeTemplate()
      ->SetIntrinsicDataProperty(Symbol::GetIterator(isolate),
                                 v8::kArrayProto_values, DontEnum);
}

// --- Document ----------------------------------------------------------------

bool IsValidElementName(const AtomicString& name) {
  if (name.empty()) return false;
  if (!name.is_8bit()) return true;
  std::string_view view = name.view8();
  char first = view[0];
  if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') ||
        first == '_' || first == ':' ||
        static_cast<unsigned char>(first) >= 0x80)) {
    return false;
  }
  for (char c : view) {
    if (IsHtmlSpace(c) || c == '<' || c == '>' || c == '/' || c == '=' ||
        c == '"' || c == '\'') {
      return false;
    }
  }
  return true;
}

// Creates an element for script: custom elements with a definition are
// constructed synchronously by running their constructor.
Element* CreateElementForScript(Isolate* isolate, Document* document,
                                Namespace ns, const AtomicString& local_name) {
  if (ns == Namespace::kHTML &&
      CustomElementRegistry::IsValidName(local_name)) {
    Window* window = document->window();
    CustomElementDefinition* definition =
        window->custom_elements()->Lookup(local_name);
    if (definition) {
      Local<Context> context = window->context(isolate);
      TryCatch try_catch(isolate);
      Local<Object> result;
      Element* element = nullptr;
      if (definition->constructor.Get(isolate)->NewInstance(context).ToLocal(
              &result)) {
        element = To<Element>(isolate, result, Interface::kElement);
        if (!element) {
          Local<Value> error =
              Exception::TypeError(v8::String::NewFromUtf8Literal(
                  isolate, "The result must implement HTMLElement interface"));
          window->ReportException(isolate, error,
                                  Exception::CreateMessage(isolate, error));
        }
      } else if (try_catch.HasTerminated()) {
        try_catch.ReThrow();
        return nullptr;
      } else {
        window->ReportException(isolate, try_catch.Exception(),
                                try_catch.Message());
      }
      if (element) return element;
      // A failed constructor yields an HTMLUnknownElement, per spec.
      Element* fallback = document->CreateElement(isolate, ns, local_name);
      fallback->set_interface(Interface::kHTMLUnknownElement);
      fallback->set_flag(Node::kIsUndefinedCustom, false);
      return fallback;
    }
  }
  return document->CreateElement(isolate, ns, local_name);
}

CALLBACK(DocumentCreateElement) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Document* document = Receiver<Document>(info);
  AtomicString name;
  if (!ToAtom(info, info[0], &name)) return;
  if (!IsValidElementName(name)) {
    ThrowDOMException(
        isolate, "InvalidCharacterError",
        "The tag name provided ('" + name.ToUtf8() + "') is not a valid name.");
    return;
  }
  if (name.is_8bit()) {
    std::string_view view = name.view8();
    for (char c : view) {
      if (c >= 'A' && c <= 'Z') {
        name = AtomicString(std::string_view(AsciiLower(view)));
        break;
      }
    }
  }
  Return(info,
         CreateElementForScript(isolate, document, Namespace::kHTML, name));
}

CALLBACK(DocumentCreateElementNS) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 2)) return;
  Document* document = Receiver<Document>(info);
  String ns_uri;
  std::string qualified_name;
  if (!ToStringOrNull(info, info[0], &ns_uri) ||
      !ToStdString(info, info[1], &qualified_name)) {
    return;
  }
  size_t colon = qualified_name.find(':');
  std::string local_name = colon == std::string::npos
                               ? qualified_name
                               : qualified_name.substr(colon + 1);
  AtomicString name = AtomicString::FromUtf8(local_name);
  if (!IsValidElementName(name)) {
    ThrowDOMException(isolate, "InvalidCharacterError",
                      "The qualified name provided ('" + qualified_name +
                          "') is not a valid name.");
    return;
  }
  Return(info, CreateElementForScript(isolate, document,
                                      NamespaceFromURI(ns_uri), name));
}

CALLBACK(DocumentCreateEvent) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  std::string name;
  if (!ToStdString(info, info[0], &name)) return;
  name = AsciiLower(name);
  static const struct {
    const char* name;
    Interface interface;
  } kLegacyNames[] = {
      {"event", Interface::kEvent},
      {"events", Interface::kEvent},
      {"htmlevents", Interface::kEvent},
      {"svgevents", Interface::kEvent},
      {"customevent", Interface::kCustomEvent},
      {"uievent", Interface::kUIEvent},
      {"uievents", Interface::kUIEvent},
      {"mouseevent", Interface::kMouseEvent},
      {"mouseevents", Interface::kMouseEvent},
      {"keyboardevent", Interface::kKeyboardEvent},
      {"focusevent", Interface::kFocusEvent},
      {"messageevent", Interface::kMessageEvent},
      {"hashchangeevent", Interface::kHashChangeEvent},
      {"compositionevent", Interface::kCompositionEvent},
      {"dragevent", Interface::kDragEvent},
  };
  for (const auto& entry : kLegacyNames) {
    if (name == entry.name) {
      Return(info,
             MakeGarbageCollected<Event>(isolate, isolate, entry.interface,
                                         AtomicString::EmptyAtom()));
      return;
    }
  }
  ThrowDOMException(isolate, "NotSupportedError",
                    "The provided event type ('" + name + "') is invalid.");
}

template <NodeList::Kind kind>
CALLBACK(GetElementsBy) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Node* root = Receiver<Node>(info);
  AtomicString name;
  if (!ToAtom(info, info[0], &name)) return;
  NodeList* list = MakeGarbageCollected<NodeList>(
      isolate, Interface::kHTMLCollection, kind, root, name);
  if (kind == NodeList::kByClassName) list->SetClassNames(name.ToUtf8());
  Return(info, list);
}

CALLBACK(DocumentImportNode) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!node) return;
  bool deep = info.Length() > 1 && info[1]->BooleanValue(isolate);
  Node* clone = node->Clone(isolate, Receiver<Document>(info), deep);
  if (!clone) {
    ThrowDOMException(isolate, "NotSupportedError",
                      "The node provided is a document, which may not be "
                      "imported.");
    return;
  }
  Return(info, clone);
}

CALLBACK(DocumentAdoptNode) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!node) return;
  if (node->IsDocument()) {
    ThrowDOMException(isolate, "NotSupportedError",
                      "The node provided is a document, which may not be "
                      "adopted.");
    return;
  }
  if (node->IsShadowRoot()) {
    ThrowDOMException(isolate, "HierarchyRequestError",
                      "The node provided is a shadow root, which may not be "
                      "adopted.");
    return;
  }
  node->Remove(isolate);
  AdoptSubtree(node, Receiver<Document>(info));
  info.GetReturnValue().Set(info[0]);
}

AdoptedStyleSheets* AdoptedOf(Node* scope) {
  return scope->IsShadowRoot()
             ? static_cast<ShadowRoot*>(scope)->adopted_style_sheets.Get()
             : static_cast<Document*>(scope)->adopted_style_sheets.Get();
}

AdoptedStyleSheets* EnsureAdopted(Isolate* isolate, Node* scope) {
  AdoptedStyleSheets* adopted = AdoptedOf(scope);
  if (adopted) return adopted;
  adopted = MakeGarbageCollected<AdoptedStyleSheets>(isolate, scope);
  if (scope->IsShadowRoot()) {
    static_cast<ShadowRoot*>(scope)->adopted_style_sheets = adopted;
  } else {
    static_cast<Document*>(scope)->adopted_style_sheets = adopted;
  }
  return adopted;
}

void InvalidateAdoptedStyles(Node* scope) {
  Document* document =
      scope->IsDocument() ? static_cast<Document*>(scope) : scope->document();
  document->InvalidateLayout();
  InvalidateAllStyles(document);
}

bool IsLengthProperty(Isolate* isolate, Local<Value> prop) {
  return prop->IsString() &&
         prop.As<v8::String>()->StringEquals(V8Name(isolate, "length"));
}

bool ParseArrayIndex(Isolate* isolate, Local<Value> prop, uint32_t* out) {
  if (!prop->IsString()) return false;
  std::string text = ToUtf8(isolate, prop.As<v8::String>());
  if (text.empty()) return false;
  if (text.size() > 1 && text[0] == '0') return false;
  uint64_t value = 0;
  for (char c : text) {
    if (c < '0' || c > '9') return false;
    value = value * 10 + (c - '0');
    if (value >= 0xFFFFFFFFu) return false;
  }
  *out = static_cast<uint32_t>(value);
  return true;
}

AdoptedStyleSheets* AdoptedFromTrapData(const CallbackInfo& info) {
  Node* scope = static_cast<Node*>(static_cast<ScriptWrappable*>(
      Object::Unwrap<kWrapperTag, Object::Wrappable>(
          info.GetIsolate(), info.Data().As<Object>())));
  return AdoptedOf(scope);
}

bool SetAdoptedElement(Isolate* isolate, AdoptedStyleSheets* adopted,
                       Local<Array> target, uint32_t index,
                       Local<Value> value) {
  auto& sheets = adopted->mutable_sheets();
  if (index > sheets.size()) {
    ThrowTypeError(isolate, "Cannot set sparse index on adoptedStyleSheets.");
    return false;
  }
  CSSStyleSheet* sheet =
      To<CSSStyleSheet>(isolate, value, Interface::kCSSStyleSheet);
  if (!sheet) {
    ThrowTypeError(isolate, "Failed to convert value to 'CSSStyleSheet'.");
    return false;
  }
  bool ok = false;
  if (!target->CreateDataProperty(isolate->GetCurrentContext(), index, value)
           .To(&ok) ||
      !ok) {
    return false;
  }
  if (index == sheets.size()) {
    sheets.push_back(sheet);
  } else {
    sheets[index] = sheet;
  }
  InvalidateAdoptedStyles(adopted->scope());
  return true;
}

bool SetAdoptedLength(Isolate* isolate, AdoptedStyleSheets* adopted,
                      Local<Array> target, Local<Value> value) {
  Local<Context> context = isolate->GetCurrentContext();
  double number = 0;
  uint32_t new_length = 0;
  if (!value->NumberValue(context).To(&number) ||
      !value->Uint32Value(context).To(&new_length)) {
    return false;
  }
  if (!std::isfinite(number) || number != static_cast<double>(new_length)) {
    isolate->ThrowException(
        Exception::RangeError(V8Name(isolate, "Invalid array length")));
    return false;
  }
  auto& sheets = adopted->mutable_sheets();
  if (new_length > sheets.size()) {
    ThrowTypeError(isolate, "Cannot grow adoptedStyleSheets length directly.");
    return false;
  }
  bool ok = false;
  if (!target
           ->Set(context, V8Name(isolate, "length"),
                 Uint32::NewFromUnsigned(isolate, new_length))
           .To(&ok) ||
      !ok) {
    return false;
  }
  if (new_length != sheets.size()) {
    sheets.resize(new_length);
    InvalidateAdoptedStyles(adopted->scope());
  }
  return true;
}

CALLBACK(AdoptedSetTrap) {
  Isolate* isolate = info.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  AdoptedStyleSheets* adopted = AdoptedFromTrapData(info);
  Local<Array> target = info[0].As<Array>();
  Local<Value> prop = info[1];
  Local<Value> value = info[2];
  if (IsLengthProperty(isolate, prop)) {
    info.GetReturnValue().Set(
        SetAdoptedLength(isolate, adopted, target, value));
    return;
  }
  uint32_t index = 0;
  if (ParseArrayIndex(isolate, prop, &index)) {
    info.GetReturnValue().Set(
        SetAdoptedElement(isolate, adopted, target, index, value));
    return;
  }
  bool ok = false;
  if (target->Set(context, prop, value).To(&ok)) {
    info.GetReturnValue().Set(ok);
  }
}

CALLBACK(AdoptedDefinePropertyTrap) {
  Isolate* isolate = info.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  AdoptedStyleSheets* adopted = AdoptedFromTrapData(info);
  Local<Array> target = info[0].As<Array>();
  Local<Value> prop = info[1];
  if (!info[2]->IsObject()) {
    info.GetReturnValue().Set(false);
    return;
  }
  Local<Object> descriptor = info[2].As<Object>();
  bool has_get = false, has_set = false;
  if (!descriptor->Has(context, V8Name(isolate, "get")).To(&has_get) ||
      !descriptor->Has(context, V8Name(isolate, "set")).To(&has_set)) {
    return;
  }
  if (has_get || has_set) {
    info.GetReturnValue().Set(false);
    return;
  }
  bool has_value = false;
  if (!descriptor->Has(context, V8Name(isolate, "value")).To(&has_value)) {
    return;
  }
  Local<Value> value = Undefined(isolate);
  if (has_value && !GetProperty(isolate, descriptor, "value", &value)) return;
  if (IsLengthProperty(isolate, prop)) {
    bool has_configurable = false;
    if (!descriptor->Has(context, V8Name(isolate, "configurable"))
             .To(&has_configurable)) {
      return;
    }
    if (has_configurable) {
      Local<Value> configurable;
      if (!GetProperty(isolate, descriptor, "configurable", &configurable)) {
        return;
      }
      if (configurable->BooleanValue(isolate)) {
        info.GetReturnValue().Set(false);
        return;
      }
    }
    bool has_writable = false;
    if (!descriptor->Has(context, V8Name(isolate, "writable"))
             .To(&has_writable)) {
      return;
    }
    if (has_writable) {
      Local<Value> writable;
      if (!GetProperty(isolate, descriptor, "writable", &writable)) return;
      if (!writable->BooleanValue(isolate)) {
        info.GetReturnValue().Set(false);
        return;
      }
    }
    info.GetReturnValue().Set(
        has_value ? SetAdoptedLength(isolate, adopted, target, value) : true);
    return;
  }
  uint32_t index = 0;
  if (ParseArrayIndex(isolate, prop, &index)) {
    for (const char* attr : {"configurable", "enumerable", "writable"}) {
      bool has_attr = false;
      if (!descriptor->Has(context, V8Name(isolate, attr)).To(&has_attr)) {
        return;
      }
      if (!has_attr) continue;
      Local<Value> attr_val;
      if (!GetProperty(isolate, descriptor, attr, &attr_val)) return;
      if (!attr_val->BooleanValue(isolate)) {
        info.GetReturnValue().Set(false);
        return;
      }
    }
    if (!has_value) {
      info.GetReturnValue().Set(false);
      return;
    }
    info.GetReturnValue().Set(
        SetAdoptedElement(isolate, adopted, target, index, value));
    return;
  }
  if (!prop->IsName()) {
    info.GetReturnValue().Set(false);
    return;
  }
  bool has_configurable = false;
  if (!descriptor->Has(context, V8Name(isolate, "configurable"))
           .To(&has_configurable)) {
    return;
  }
  if (has_configurable) {
    Local<Value> configurable;
    if (!GetProperty(isolate, descriptor, "configurable", &configurable)) {
      return;
    }
    if (!configurable->BooleanValue(isolate)) {
      info.GetReturnValue().Set(false);
      return;
    }
  }
  bool ok = false;
  if (target->CreateDataProperty(context, prop.As<Name>(), value).To(&ok)) {
    info.GetReturnValue().Set(ok);
  }
}

CALLBACK(AdoptedDeletePropertyTrap) {
  Isolate* isolate = info.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  AdoptedStyleSheets* adopted = AdoptedFromTrapData(info);
  Local<Array> target = info[0].As<Array>();
  Local<Value> prop = info[1];
  if (IsLengthProperty(isolate, prop)) {
    info.GetReturnValue().Set(false);
    return;
  }
  uint32_t index = 0;
  if (ParseArrayIndex(isolate, prop, &index)) {
    auto& sheets = adopted->mutable_sheets();
    if (index < sheets.size()) {
      bool ok = false;
      if (!target->Delete(context, index).To(&ok) || !ok) {
        info.GetReturnValue().Set(false);
        return;
      }
      sheets[index] = nullptr;
      InvalidateAdoptedStyles(adopted->scope());
    }
    info.GetReturnValue().Set(true);
    return;
  }
  bool ok = false;
  if (target->Delete(context, prop).To(&ok)) {
    info.GetReturnValue().Set(ok);
  }
}

CALLBACK(AdoptedPreventExtensionsTrap) { info.GetReturnValue().Set(false); }

Local<Proxy> EnsureAdoptedProxy(Isolate* isolate, Node* scope) {
  AdoptedStyleSheets* adopted = EnsureAdopted(isolate, scope);
  if (!adopted->proxy().IsEmpty()) return adopted->proxy().Get(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  const auto& sheets = adopted->sheets();
  Local<Array> target = Array::New(isolate, static_cast<int>(sheets.size()));
  for (uint32_t i = 0; i < sheets.size(); ++i) {
    if (sheets[i]) {
      USE(target->CreateDataProperty(context, i,
                                     ToV8(isolate, sheets[i].Get())));
    }
  }
  Local<Value> scope_wrapper = ToV8(isolate, scope);
  Local<Object> handler = Object::New(isolate);
  auto add_trap = [&](const char* name, FunctionCallback cb) {
    Local<Function> fn =
        Function::New(context, cb, scope_wrapper).ToLocalChecked();
    USE(handler->CreateDataProperty(context, V8Name(isolate, name), fn));
  };
  add_trap("set", AdoptedSetTrap);
  add_trap("defineProperty", AdoptedDefinePropertyTrap);
  add_trap("deleteProperty", AdoptedDeletePropertyTrap);
  add_trap("preventExtensions", AdoptedPreventExtensionsTrap);
  Local<Proxy> proxy = Proxy::New(context, target, handler).ToLocalChecked();
  adopted->target().Reset(isolate, target);
  adopted->proxy().Reset(isolate, proxy);
  return proxy;
}

bool CollectAdoptedStyleSheets(Isolate* isolate, Local<Value> value,
                               LocalVector<Value>* out) {
  Local<Context> context = isolate->GetCurrentContext();
  auto append_sheet = [&](Local<Value> item) {
    if (!To<CSSStyleSheet>(isolate, item, Interface::kCSSStyleSheet)) {
      ThrowTypeError(isolate,
                     "Failed to set the 'adoptedStyleSheets' property: "
                     "Failed to convert value to 'CSSStyleSheet'.");
      return false;
    }
    out->push_back(item);
    return true;
  };
  if (value->IsArray()) {
    Local<Array> array = value.As<Array>();
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; ++i) {
      Local<Value> item;
      if (!array->Get(context, i).ToLocal(&item)) return false;
      if (!append_sheet(item)) return false;
    }
    return true;
  }
  if (!value->IsObject()) {
    ThrowTypeError(isolate,
                   "Failed to set the 'adoptedStyleSheets' property: "
                   "The provided value cannot be converted to a sequence.");
    return false;
  }
  Local<Object> object = value.As<Object>();
  Local<Value> iterator_fn;
  if (!object->Get(context, Symbol::GetIterator(isolate))
           .ToLocal(&iterator_fn)) {
    return false;
  }
  if (!iterator_fn->IsFunction()) {
    ThrowTypeError(isolate,
                   "Failed to set the 'adoptedStyleSheets' property: "
                   "The provided value cannot be converted to a sequence.");
    return false;
  }
  Local<Value> iterator_val;
  if (!iterator_fn.As<Function>()
           ->Call(context, object, 0, nullptr)
           .ToLocal(&iterator_val)) {
    return false;
  }
  if (!iterator_val->IsObject()) {
    ThrowTypeError(isolate,
                   "Result of the Symbol.iterator method is not an "
                   "object.");
    return false;
  }
  Local<Object> iterator = iterator_val.As<Object>();
  Local<Value> next_fn;
  if (!GetProperty(isolate, iterator, "next", &next_fn)) return false;
  if (!next_fn->IsFunction()) {
    ThrowTypeError(isolate, "Iterator next is not a function.");
    return false;
  }
  while (true) {
    Local<Value> step_val;
    if (!next_fn.As<Function>()
             ->Call(context, iterator, 0, nullptr)
             .ToLocal(&step_val)) {
      return false;
    }
    if (!step_val->IsObject()) {
      ThrowTypeError(isolate, "Iterator result is not an object.");
      return false;
    }
    Local<Object> step = step_val.As<Object>();
    Local<Value> done;
    if (!GetProperty(isolate, step, "done", &done)) return false;
    if (done->BooleanValue(isolate)) break;
    Local<Value> item;
    if (!GetProperty(isolate, step, "value", &item)) return false;
    if (!append_sheet(item)) return false;
  }
  return true;
}

CALLBACK(DocumentFragmentConstructor) {
  if (!RequireConstructCall(info)) return;
  Isolate* isolate = info.GetIsolate();
  DocumentFragment* fragment =
      Window::Current(isolate)->document()->CreateDocumentFragment(isolate);
  Object::Wrap<kWrapperTag>(isolate, info.This(), fragment);
  fragment->set_wrapper(isolate, info.This());
}

void InstallDocumentBindings(Isolate* isolate) {
  const Interface kDocument = Interface::kDocument;
  DefineAccessor(isolate, kDocument, "documentElement",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Document>(info)->document_element());
                 });
  DefineAccessor(isolate, kDocument, "all", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Return(info,
           MakeGarbageCollected<NodeList>(
               isolate, Interface::kHTMLAllCollection, NodeList::kByTagName,
               Receiver<Document>(info), AtomicString("*")));
  });
  DefineAccessor(
      isolate, kDocument, "body",
      [](const CallbackInfo& info) {
        Return(info, Receiver<Document>(info)->body());
      },
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        Document* document = Receiver<Document>(info);
        Element* body = To<Element>(isolate, info[0], Interface::kElement);
        Element* html = document->document_element();
        if (!body || !html) {
          ThrowDOMException(isolate, "HierarchyRequestError",
                            "The new body element is not valid.");
          return;
        }
        ExceptionState exception_state;
        if (Element* old = document->body()) {
          html->ReplaceChild(isolate, body, old, &exception_state);
        } else {
          html->AppendChild(isolate, body, &exception_state);
        }
        ThrowIfFailed(isolate, exception_state);
      });
  DefineAccessor(isolate, kDocument, "head", [](const CallbackInfo& info) {
    Return(info, Receiver<Document>(info)->head());
  });
  DefineAccessor(isolate, kDocument, "doctype", [](const CallbackInfo& info) {
    Document* document = Receiver<Document>(info);
    for (Node* n = document->first(); n; n = n->next_sibling()) {
      if (n->node_type() == Node::kDocumentTypeNode) {
        Return(info, n);
        return;
      }
    }
    info.GetReturnValue().SetNull();
  });
  DefineAccessor(
      isolate, kDocument, "title",
      [](const CallbackInfo& info) {
        Document* document = Receiver<Document>(info);
        if (Element* head = document->head()) {
          for (Node* n = head->first(); n; n = n->next_sibling()) {
            if (n->IsElement() && static_cast<Element*>(n)->HasTag("title")) {
              Return(info, n->TextContent());
              return;
            }
          }
        }
        Return(info,
               document->title.is_null() ? String::Empty() : document->title);
      },
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        Document* document = Receiver<Document>(info);
        String title;
        if (!ToString(info, info[0], &title)) return;
        document->title = title;
        if (Element* head = document->head()) {
          for (Node* n = head->first(); n; n = n->next_sibling()) {
            if (n->IsElement() && static_cast<Element*>(n)->HasTag("title")) {
              n->SetTextContent(isolate, title);
              return;
            }
          }
          Element* element = document->CreateElement(isolate, Namespace::kHTML,
                                                     AtomicString("title"));
          element->SetTextContent(isolate, title);
          ExceptionState exception_state;
          head->AppendChild(isolate, element, &exception_state);
        }
      });
  DefineAccessor(isolate, kDocument, "readyState",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Document>(info)->ready_state);
                 });
  DefineAccessor(isolate, kDocument, "defaultView",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Document>(info)->default_view());
                 });
  DefineAccessor(
      isolate, kDocument, "location",
      [](const CallbackInfo& info) {
        Window* view = Receiver<Document>(info)->default_view();
        Return(info, view ? view->location() : nullptr);
      },
      [](const CallbackInfo& info) {
        Window* view = Receiver<Document>(info)->default_view();
        std::string url;
        if (!view || !ToStdString(info, info[0], &url)) return;
        view->location()->Navigate(info.GetIsolate(), url);
      });
  auto url_getter = [](const CallbackInfo& info) {
    Document* document = Receiver<Document>(info);
    Window* view = document->default_view();
    info.GetReturnValue().Set(ToV8StringFromUtf8(
        info.GetIsolate(),
        view ? view->location()->url().Href() : "about:blank"));
  };
  DefineAccessor(isolate, kDocument, "URL", url_getter);
  DefineAccessor(isolate, kDocument, "documentURI", url_getter);
  DefineAccessor(isolate, kDocument, "activeElement",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Document>(info)->active_element());
                 });
  DefineAccessor(isolate, kDocument, "scrollingElement",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Document>(info)->document_element());
                 });
  DefineAccessor(
      isolate, kDocument, "cookie",
      [](const CallbackInfo& info) {
        Document* document = Receiver<Document>(info);
        Return(info,
               document->cookie.is_null() ? String::Empty() : document->cookie);
      },
      [](const CallbackInfo& info) {
        // Keeps "name=value" pairs; attributes such as path and expires are
        // dropped.
        Document* document = Receiver<Document>(info);
        std::string cookie;
        if (!ToStdString(info, info[0], &cookie)) return;
        std::string pair = cookie.substr(0, cookie.find(';'));
        std::string name = pair.substr(0, pair.find('='));
        std::string existing = document->cookie.ToUtf8();
        std::string updated;
        size_t i = 0;
        while (i < existing.size()) {
          size_t end = existing.find("; ", i);
          if (end == std::string::npos) end = existing.size();
          std::string entry = existing.substr(i, end - i);
          if (entry.substr(0, entry.find('=')) != name) {
            if (!updated.empty()) updated += "; ";
            updated += entry;
          }
          i = end + 2;
        }
        if (!updated.empty()) updated += "; ";
        updated += pair;
        document->cookie = String::FromUtf8(updated);
      });
  DefineAccessor(isolate, kDocument, "currentScript",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Document>(info)->current_script.Get());
                 });
  DefineAccessor(isolate, kDocument, "hidden", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(false);
  });
  static const struct {
    const char* name;
    const char* value;
  } kStringConstants[] = {
      {"visibilityState", "visible"},
      {"compatMode", "CSS1Compat"},
      {"characterSet", "UTF-8"},
      {"charset", "UTF-8"},
      {"inputEncoding", "UTF-8"},
      {"contentType", "text/html"},
      {"referrer", ""},
      {"dir", ""},
      {"designMode", "off"},
  };
  for (const auto& constant : kStringConstants) {
    DefineAccessor(
        isolate, kDocument, constant.name,
        [](const CallbackInfo& info) {
          info.GetReturnValue().Set(info.Data().As<Value>());
        },
        nullptr, V8Name(isolate, constant.value));
  }

  DefineMethod(isolate, kDocument, "createElement", DocumentCreateElement, 1);
  DefineMethod(isolate, kDocument, "createElementNS", DocumentCreateElementNS,
               2);
  DefineMethod(
      isolate, kDocument, "createTextNode",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        String data;
        if (!ToString(info, info[0], &data)) return;
        Return(info, Receiver<Document>(info)->CreateTextNode(info.GetIsolate(),
                                                              data));
      },
      1);
  DefineMethod(
      isolate, kDocument, "createComment",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        String data;
        if (!ToString(info, info[0], &data)) return;
        Return(info, Receiver<Document>(info)->CreateComment(info.GetIsolate(),
                                                             data));
      },
      1);
  DefineMethod(isolate, kDocument, "createDocumentFragment",
               [](const CallbackInfo& info) {
                 Return(info, Receiver<Document>(info)->CreateDocumentFragment(
                                  info.GetIsolate()));
               });
  DefineMethod(isolate, kDocument, "createEvent", DocumentCreateEvent, 1);
  DefineMethod(
      isolate, kDocument, "getElementById",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        AtomicString id;
        if (!ToAtom(info, info[0], &id)) return;
        Return(info, Receiver<Document>(info)->GetElementById(id));
      },
      1);
  DefineMethod(
      isolate, Interface::kDocumentFragment, "getElementById",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        AtomicString id;
        if (!ToAtom(info, info[0], &id)) return;
        Return(info, Receiver<DocumentFragment>(info)->FindElementById(id));
      },
      1);
  for (Interface interface : {kDocument, Interface::kElement}) {
    DefineMethod(isolate, interface, "getElementsByTagName",
                 GetElementsBy<NodeList::kByTagName>, 1);
    DefineMethod(isolate, interface, "getElementsByClassName",
                 GetElementsBy<NodeList::kByClassName>, 1);
  }
  DefineMethod(
      isolate, kDocument, "getElementsByName",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 1)) return;
        AtomicString name;
        if (!ToAtom(info, info[0], &name)) return;
        static const AtomicString* name_atom = new AtomicString("name");
        Document* document = Receiver<Document>(info);
        MOCK_DOM_LOCAL_NODES
        std::vector<Element*> results;
        for (Node* n = document->first(); n; n = n->NextInPreOrder(document)) {
          if (n->IsElement() &&
              static_cast<Element*>(n)->GetAttribute(*name_atom) == name) {
            results.push_back(static_cast<Element*>(n));
          }
        }
        Return(info, NodeList::CreateStatic(isolate, results));
      },
      1);
  DefineMethod(isolate, kDocument, "importNode", DocumentImportNode, 1);
  DefineMethod(isolate, kDocument, "adoptNode", DocumentAdoptNode, 1);
  DefineMethod(isolate, kDocument, "hasFocus", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(true);
  });
  DefineMethod(
      isolate, kDocument, "elementFromPoint",
      [](const CallbackInfo& info) {
        // There is no layout to hit-test against; like a browser asked about
        // a point over empty page background, answer with the body.
        Document* document = Receiver<Document>(info);
        Element* body = document->body();
        Return(info, body ? body : document->document_element());
      },
      2);

  SetConstructor(isolate, Interface::kDocumentFragment,
                 DocumentFragmentConstructor);
  DefineAccessor(isolate, Interface::kShadowRoot, "host",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<ShadowRoot>(info)->host());
                 });
  DefineAccessor(
      isolate, Interface::kShadowRoot, "mode", [](const CallbackInfo& info) {
        Return(info,
               AtomicString(Receiver<ShadowRoot>(info)->open() ? "open"
                                                               : "closed"));
      });
  DefineAccessor(
      isolate, Interface::kShadowRoot, "innerHTML",
      [](const CallbackInfo& info) {
        std::string out;
        SerializeChildren(Receiver<ShadowRoot>(info), &out);
        Return(info, String::FromUtf8(out));
      },
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        ShadowRoot* root = Receiver<ShadowRoot>(info);
        std::string html;
        if (!info[0]->IsNull() && !ToStdString(info, info[0], &html)) return;
        CEReactionsScope reactions(isolate);
        DocumentFragment* fragment =
            root->document()->CreateDocumentFragment(isolate);
        ParseHTMLFragment(isolate, fragment, root->host(), html);
        root->RemoveChildren(isolate);
        ExceptionState exception_state;
        root->AppendChild(isolate, fragment, &exception_state);
      });
  DefineAccessor(
      isolate, Interface::kShadowRoot, "activeElement",
      [](const CallbackInfo& info) {
        ShadowRoot* root = Receiver<ShadowRoot>(info);
        Element* focused = root->document()->focused_element();
        Return(info, focused && root->Contains(focused) ? focused : nullptr);
      });
  for (Interface interface : {kDocument, Interface::kShadowRoot}) {
    // The sheets live in a C++ vector on the tree scope so the cascade reads
    // them without calling into JS; JS sees an ObservableArray-like Proxy.
    DefineAccessor(
        isolate, interface, "adoptedStyleSheets",
        [](const CallbackInfo& info) {
          info.GetReturnValue().Set(
              EnsureAdoptedProxy(info.GetIsolate(), Receiver<Node>(info)));
        },
        [](const CallbackInfo& info) {
          Isolate* isolate = info.GetIsolate();
          Node* scope = Receiver<Node>(info);
          LocalVector<Value> values(isolate);
          if (!CollectAdoptedStyleSheets(isolate, info[0], &values)) return;
          AdoptedStyleSheets* adopted = EnsureAdopted(isolate, scope);
          auto& sheets = adopted->mutable_sheets();
          sheets.clear();
          sheets.reserve(values.size());
          for (Local<Value> value : values) {
            sheets.push_back(
                To<CSSStyleSheet>(isolate, value, Interface::kCSSStyleSheet));
          }
          if (!adopted->target().IsEmpty()) {
            Local<Context> context = isolate->GetCurrentContext();
            Local<Array> target = adopted->target().Get(isolate);
            USE(target->Set(context, V8Name(isolate, "length"),
                            Uint32::NewFromUnsigned(isolate, 0)));
            for (uint32_t i = 0; i < values.size(); ++i) {
              USE(target->CreateDataProperty(context, i, values[i]));
            }
          }
          InvalidateAdoptedStyles(scope);
        });
  }
}

// --- DOMException ------------------------------------------------------------

CALLBACK(DOMExceptionConstructor) {
  if (!RequireConstructCall(info)) return;
  Isolate* isolate = info.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  std::string message, name = "Error";
  if (info.Length() > 0 && !info[0]->IsUndefined() &&
      !ToStdString(info, info[0], &message)) {
    return;
  }
  if (info.Length() > 1 && !info[1]->IsUndefined() &&
      !ToStdString(info, info[1], &name)) {
    return;
  }
  Local<Object> self = info.This();
  USE(self->DefineOwnProperty(context, V8Name(isolate, "message"),
                              ToV8StringFromUtf8(isolate, message), DontEnum));
  USE(self->DefineOwnProperty(context, V8Name(isolate, "name"),
                              ToV8StringFromUtf8(isolate, name), DontEnum));
  USE(self->DefineOwnProperty(
      context, V8Name(isolate, "code"),
      Integer::New(isolate, LegacyDOMExceptionCode(name)), DontEnum));
}

#undef CALLBACK

}  // namespace

void InstallCoreBindings(Isolate* isolate) {
  InstallEventBindings(isolate);
  InstallNodeBindings(isolate);
  InstallCharacterDataBindings(isolate);
  InstallCollectionBindings(isolate);
  InstallDocumentBindings(isolate);
  SetConstructor(isolate, Interface::kDOMException, DOMExceptionConstructor);
}

void InitializeRealmForCore(Isolate* isolate, Local<Context> context) {
  Local<Object> global = context->Global();
  auto get = [&](Local<Object> object, const char* name) -> Local<Object> {
    return object->Get(context, V8Name(isolate, name))
        .ToLocalChecked()
        .As<Object>();
  };
  Local<Object> array_prototype = get(get(global, "Array"), "prototype");
  Local<Object> error_prototype = get(get(global, "Error"), "prototype");

  // Web IDL value iterators: collections reuse the Array methods (NodeList
  // installs them via SetIntrinsicDataProperty on its PrototypeTemplate).
  Local<Value> iterator =
      array_prototype->Get(context, Symbol::GetIterator(isolate))
          .ToLocalChecked();
  static const char* const kIterableMethods[] = {"forEach", "entries", "keys",
                                                 "values"};
  for (Interface interface :
       {Interface::kDOMTokenList, Interface::kHTMLCollection,
        Interface::kNamedNodeMap, Interface::kDOMRectList,
        Interface::kDOMStringList}) {
    Local<Object> prototype = get(InterfaceTemplate(isolate, interface)
                                      ->GetFunction(context)
                                      .ToLocalChecked(),
                                  "prototype");
    USE(prototype->DefineOwnProperty(context, Symbol::GetIterator(isolate),
                                     iterator, DontEnum));
    if (interface == Interface::kHTMLCollection ||
        interface == Interface::kNamedNodeMap ||
        interface == Interface::kDOMStringList) {
      continue;
    }
    for (const char* name : kIterableMethods) {
      USE(prototype->Set(context, V8Name(isolate, name),
                         get(array_prototype, name)));
    }
  }

  // DOMException.prototype inherits from Error.prototype.
  Local<Object> dom_exception_prototype =
      get(InterfaceTemplate(isolate, Interface::kDOMException)
              ->GetFunction(context)
              .ToLocalChecked(),
          "prototype");
  USE(dom_exception_prototype->SetPrototype(context, error_prototype));
}

}  // namespace mock_dom
}  // namespace v8
