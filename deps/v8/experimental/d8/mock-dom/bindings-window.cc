// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bindings for the global object (Window) and the objects reachable from it:
// location, history, storage, custom elements, observers, message channels.

#include <cmath>
#include <cstdlib>
#include <map>

#include "experimental/d8/mock-dom/bindings.h"
#include "experimental/d8/mock-dom/css.h"
#include "experimental/d8/mock-dom/html.h"
#include "experimental/d8/mock-dom/isolate-data.h"
#include "experimental/d8/mock-dom/layout.h"
#include "experimental/d8/mock-dom/objects.h"
#include "include/v8-container.h"
#include "include/v8-exception.h"
#include "include/v8-microtask-queue.h"
#include "include/v8-primitive.h"
#include "include/v8-promise.h"
#include "include/v8-typed-array.h"
#include "src/base/utils/random-number-generator.h"

namespace v8 {
namespace mock_dom {

namespace {

#define CALLBACK(name) void name(const CallbackInfo& info)

Window* CurrentWindow(const CallbackInfo& info) {
  return Window::Current(info.GetIsolate());
}

void WrapThis(const CallbackInfo& info, ScriptWrappable* object) {
  Object::Wrap<kWrapperTag>(info.GetIsolate(), info.This(), object);
  object->set_wrapper(info.GetIsolate(), info.This());
}

// --- Location ----------------------------------------------------------------

void InstallLocationBindings(Isolate* isolate) {
  const Interface kLocation = Interface::kLocation;
  struct Part {
    const char* name;
    std::string (*get)(const ParsedURL&);
  };
  static const Part kParts[] = {
      {"protocol", [](const ParsedURL& u) { return u.Protocol(); }},
      {"host", [](const ParsedURL& u) { return u.Host(); }},
      {"hostname", [](const ParsedURL& u) { return u.hostname; }},
      {"port", [](const ParsedURL& u) { return u.port; }},
      {"pathname", [](const ParsedURL& u) { return u.pathname; }},
      {"search", [](const ParsedURL& u) { return u.Search(); }},
      {"origin", [](const ParsedURL& u) { return u.Origin(); }},
  };
  for (size_t i = 0; i < arraysize(kParts); ++i) {
    DefineAccessor(
        isolate, kLocation, kParts[i].name,
        [](const CallbackInfo& info) {
          info.GetReturnValue().Set(ToV8StringFromUtf8(
              info.GetIsolate(),
              kParts[DataInt(info)].get(Receiver<Location>(info)->url())));
        },
        [](const CallbackInfo& info) {
          Receiver<Location>(info)->WrapperWindow()->ReportUnsupported(
              info.GetIsolate(), "assigning to a Location URL component");
        },
        Integer::New(isolate, static_cast<int>(i)));
  }
  auto href_getter = [](const CallbackInfo& info) {
    info.GetReturnValue().Set(ToV8StringFromUtf8(
        info.GetIsolate(), Receiver<Location>(info)->url().Href()));
  };
  auto navigate = [](const CallbackInfo& info) {
    std::string url;
    if (info.Length() < 1 || !ToStdString(info, info[0], &url)) return;
    Receiver<Location>(info)->Navigate(info.GetIsolate(), url);
  };
  DefineAccessor(isolate, kLocation, "href", href_getter, navigate);
  DefineMethod(isolate, kLocation, "toString", href_getter);
  DefineMethod(isolate, kLocation, "assign", navigate, 1);
  DefineMethod(isolate, kLocation, "replace", navigate, 1);
  DefineMethod(isolate, kLocation, "reload", [](const CallbackInfo& info) {
    Receiver<Location>(info)->WrapperWindow()->ReportUnsupported(
        info.GetIsolate(), "location.reload()");
  });
  DefineAccessor(
      isolate, kLocation, "hash",
      [](const CallbackInfo& info) {
        info.GetReturnValue().Set(ToV8StringFromUtf8(
            info.GetIsolate(), Receiver<Location>(info)->url().Hash()));
      },
      [](const CallbackInfo& info) {
        std::string hash;
        if (!ToStdString(info, info[0], &hash)) return;
        Receiver<Location>(info)->SetHash(info.GetIsolate(), hash);
      });
}

// --- History -----------------------------------------------------------------

template <bool push>
CALLBACK(HistoryPushOrReplace) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 2)) return;
  History* history = Receiver<History>(info);
  std::string url;
  bool has_url = info.Length() > 2 && !info[2]->IsNullOrUndefined();
  if (has_url && !ToStdString(info, info[2], &url)) return;
  ExceptionState exception_state;
  history->PushOrReplace(isolate, push, info[0], has_url, url,
                         &exception_state);
  ThrowIfFailed(isolate, exception_state);
}

void InstallHistoryBindings(Isolate* isolate) {
  const Interface kHistory = Interface::kHistory;
  DefineAccessor(isolate, kHistory, "length", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(Receiver<History>(info)->length());
  });
  DefineAccessor(isolate, kHistory, "state", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        Receiver<History>(info)->State(info.GetIsolate()));
  });
  DefineAccessor(
      isolate, kHistory, "scrollRestoration",
      [](const CallbackInfo& info) { Return(info, AtomicString("auto")); },
      [](const CallbackInfo& info) {});
  DefineMethod(isolate, kHistory, "pushState", HistoryPushOrReplace<true>, 2);
  DefineMethod(isolate, kHistory, "replaceState", HistoryPushOrReplace<false>,
               2);
  DefineMethod(isolate, kHistory, "back", [](const CallbackInfo& info) {
    Receiver<History>(info)->Go(info.GetIsolate(), -1);
  });
  DefineMethod(isolate, kHistory, "forward", [](const CallbackInfo& info) {
    Receiver<History>(info)->Go(info.GetIsolate(), 1);
  });
  DefineMethod(isolate, kHistory, "go", [](const CallbackInfo& info) {
    int delta = 0;
    if (info.Length() > 0 && !ToInt(info, info[0], &delta)) return;
    Receiver<History>(info)->Go(info.GetIsolate(), delta);
  });
}

// --- Storage -----------------------------------------------------------------

Storage* StorageHolder(Isolate* isolate, Local<Object> holder) {
  return static_cast<Storage*>(static_cast<ScriptWrappable*>(
      Object::Unwrap<kWrapperTag, Object::Wrappable>(isolate, holder)));
}

Intercepted StorageNamedGetter(Local<Name> property,
                               const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  String value = StorageHolder(isolate, info.Holder())
                     ->GetItem(ToDOMString(isolate, property.As<v8::String>()));
  if (value.is_null()) return Intercepted::kNo;
  info.GetReturnValue().Set(ToV8String(isolate, value));
  return Intercepted::kYes;
}

Intercepted StorageNamedSetter(Local<Name> property, Local<Value> value,
                               const PropertyCallbackInfo<Boolean>& info) {
  Isolate* isolate = info.GetIsolate();
  Local<v8::String> string;
  if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&string)) {
    return Intercepted::kYes;
  }
  StorageHolder(isolate, info.Holder())
      ->SetItem(ToDOMString(isolate, property.As<v8::String>()),
                ToDOMString(isolate, string));
  return Intercepted::kYes;
}

Intercepted StorageNamedDeleter(Local<Name> property,
                                const PropertyCallbackInfo<Boolean>& info) {
  Isolate* isolate = info.GetIsolate();
  StorageHolder(isolate, info.Holder())
      ->RemoveItem(ToDOMString(isolate, property.As<v8::String>()));
  info.GetReturnValue().Set(true);
  return Intercepted::kYes;
}

void StorageEnumerator(const PropertyCallbackInfo<Array>& info) {
  Isolate* isolate = info.GetIsolate();
  Storage* storage = StorageHolder(isolate, info.Holder());
  LocalVector<Value> keys(isolate);
  for (uint32_t i = 0; i < storage->length(); ++i) {
    keys.push_back(ToV8String(isolate, storage->Key(i)));
  }
  info.GetReturnValue().Set(Array::New(isolate, keys.data(), keys.size()));
}

void InstallStorageBindings(Isolate* isolate) {
  const Interface kStorage = Interface::kStorage;
  InterfaceTemplate(isolate, kStorage)
      ->InstanceTemplate()
      ->SetHandler(NamedPropertyHandlerConfiguration(
          StorageNamedGetter, StorageNamedSetter, nullptr, StorageNamedDeleter,
          StorageEnumerator, Local<Value>(),
          static_cast<PropertyHandlerFlags>(
              static_cast<int>(PropertyHandlerFlags::kNonMasking) |
              static_cast<int>(PropertyHandlerFlags::kOnlyInterceptStrings))));
  DefineAccessor(isolate, kStorage, "length", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(Receiver<Storage>(info)->length());
  });
  DefineMethod(
      isolate, kStorage, "key",
      [](const CallbackInfo& info) {
        double index = 0;
        if (info.Length() > 0 && !ToNumber(info, info[0], &index)) return;
        ReturnOrNull(info, index < 0 ? String()
                                     : Receiver<Storage>(info)->Key(
                                           static_cast<uint32_t>(index)));
      },
      1);
  DefineMethod(
      isolate, kStorage, "getItem",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        String key;
        if (!ToString(info, info[0], &key)) return;
        ReturnOrNull(info, Receiver<Storage>(info)->GetItem(key));
      },
      1);
  DefineMethod(
      isolate, kStorage, "setItem",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 2)) return;
        String key, value;
        if (!ToString(info, info[0], &key) ||
            !ToString(info, info[1], &value)) {
          return;
        }
        Receiver<Storage>(info)->SetItem(key, value);
      },
      2);
  DefineMethod(
      isolate, kStorage, "removeItem",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        String key;
        if (!ToString(info, info[0], &key)) return;
        Receiver<Storage>(info)->RemoveItem(key);
      },
      1);
  DefineMethod(isolate, kStorage, "clear", [](const CallbackInfo& info) {
    Receiver<Storage>(info)->Clear();
  });
}

// --- customElements ----------------------------------------------------------

void InstallCustomElementBindings(Isolate* isolate) {
  const Interface kRegistry = Interface::kCustomElementRegistry;
  DefineMethod(
      isolate, kRegistry, "define",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 2)) return;
        AtomicString name;
        if (!ToAtom(info, info[0], &name)) return;
        if (!info[1]->IsFunction()) {
          ThrowTypeError(isolate, "parameter 2 is not of type 'Function'.");
          return;
        }
        if (info.Length() > 2 && info[2]->IsObject()) {
          Local<Value> extends;
          if (!GetProperty(isolate, info[2].As<Object>(), "extends",
                           &extends)) {
            return;
          }
          if (!extends->IsUndefined()) {
            ThrowDOMException(isolate, "NotSupportedError",
                              "mock-dom: customized built-in elements are not "
                              "supported.");
            return;
          }
        }
        ExceptionState exception_state;
        Receiver<CustomElementRegistry>(info)->Define(
            isolate, name, info[1].As<Function>(), &exception_state);
        ThrowIfFailed(isolate, exception_state);
      },
      2);
  DefineMethod(
      isolate, kRegistry, "get",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        AtomicString name;
        if (!ToAtom(info, info[0], &name)) return;
        CustomElementDefinition* definition =
            Receiver<CustomElementRegistry>(info)->Lookup(name);
        if (definition) {
          info.GetReturnValue().Set(
              definition->constructor.Get(info.GetIsolate()));
        }
      },
      1);
  DefineMethod(
      isolate, kRegistry, "getName",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        CustomElementDefinition* definition =
            Receiver<CustomElementRegistry>(info)->LookupByConstructor(
                info.GetIsolate(), info[0]);
        if (definition) {
          Return(info, definition->name);
        } else {
          info.GetReturnValue().SetNull();
        }
      },
      1);
  DefineMethod(
      isolate, kRegistry, "whenDefined",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        AtomicString name;
        if (!ToAtom(info, info[0], &name)) return;
        info.GetReturnValue().Set(
            Receiver<CustomElementRegistry>(info)->WhenDefined(
                info.GetIsolate(), name));
      },
      1);
  DefineMethod(
      isolate, kRegistry, "upgrade",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        Node* root = ToNodeOrThrow(info, info[0], "parameter 1");
        if (!root) return;
        CEReactionsScope reactions(info.GetIsolate());
        Receiver<CustomElementRegistry>(info)->UpgradeSubtree(info.GetIsolate(),
                                                              root);
      },
      1);
}

// --- MutationObserver --------------------------------------------------------

CALLBACK(MutationObserverConstructor) {
  if (!RequireConstructCall(info) || !RequireArguments(info, 1)) return;
  Isolate* isolate = info.GetIsolate();
  if (!info[0]->IsFunction()) {
    ThrowTypeError(isolate, "parameter 1 is not of type 'Function'.");
    return;
  }
  WrapThis(info, MakeGarbageCollected<MutationObserver>(
                     isolate, isolate, info[0].As<Function>()));
}

CALLBACK(MutationObserverObserve) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Node* target = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!target) return;
  MutationObserverOptions options;
  bool has_attributes = false, has_character_data = false;
  if (info.Length() > 1 && info[1]->IsObject()) {
    Local<Object> init = info[1].As<Object>();
    Local<Value> value;
    auto read_bool = [&](const char* name, bool* out, bool* present) {
      if (!GetProperty(isolate, init, name, &value)) return false;
      if (!value->IsUndefined()) {
        *out = value->BooleanValue(isolate);
        if (present) *present = true;
      }
      return true;
    };
    if (!read_bool("childList", &options.child_list, nullptr) ||
        !read_bool("attributes", &options.attributes, &has_attributes) ||
        !read_bool("characterData", &options.character_data,
                   &has_character_data) ||
        !read_bool("subtree", &options.subtree, nullptr) ||
        !read_bool("attributeOldValue", &options.attribute_old_value,
                   nullptr) ||
        !read_bool("characterDataOldValue", &options.character_data_old_value,
                   nullptr)) {
      return;
    }
    if (!GetProperty(isolate, init, "attributeFilter", &value)) return;
    if (value->IsArray()) {
      options.has_attribute_filter = true;
      Local<Array> filter = value.As<Array>();
      Local<Context> context = isolate->GetCurrentContext();
      for (uint32_t i = 0; i < filter->Length(); ++i) {
        Local<Value> item;
        AtomicString name;
        if (!filter->Get(context, i).ToLocal(&item) ||
            !ToAtom(info, item, &name)) {
          return;
        }
        options.attribute_filter.push_back(name);
      }
    }
  }
  if (!has_attributes &&
      (options.attribute_old_value || options.has_attribute_filter)) {
    options.attributes = true;
  }
  if (!has_character_data && options.character_data_old_value) {
    options.character_data = true;
  }
  if (!options.child_list && !options.attributes && !options.character_data) {
    ThrowTypeError(isolate,
                   "The options object must set at least one of 'attributes', "
                   "'characterData', or 'childList' to true.");
    return;
  }
  Receiver<MutationObserver>(info)->Observe(isolate, target, options);
}

void InstallMutationObserverBindings(Isolate* isolate) {
  const Interface kObserver = Interface::kMutationObserver;
  SetConstructor(isolate, kObserver, MutationObserverConstructor, 1);
  DefineMethod(isolate, kObserver, "observe", MutationObserverObserve, 1);
  DefineMethod(isolate, kObserver, "disconnect", [](const CallbackInfo& info) {
    Receiver<MutationObserver>(info)->Disconnect();
  });
  DefineMethod(isolate, kObserver, "takeRecords", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        Receiver<MutationObserver>(info)->TakeRecords(info.GetIsolate()));
  });

  const Interface kRecord = Interface::kMutationRecord;
  DefineAccessor(isolate, kRecord, "type", [](const CallbackInfo& info) {
    Return(info, Receiver<MutationRecord>(info)->type);
  });
  DefineAccessor(isolate, kRecord, "target", [](const CallbackInfo& info) {
    Return(info, Receiver<MutationRecord>(info)->target.Get());
  });
  DefineAccessor(isolate, kRecord, "addedNodes", [](const CallbackInfo& info) {
    Return(info, Receiver<MutationRecord>(info)->added_nodes.Get());
  });
  DefineAccessor(
      isolate, kRecord, "removedNodes", [](const CallbackInfo& info) {
        Return(info, Receiver<MutationRecord>(info)->removed_nodes.Get());
      });
  DefineAccessor(
      isolate, kRecord, "previousSibling", [](const CallbackInfo& info) {
        Return(info, Receiver<MutationRecord>(info)->previous_sibling.Get());
      });
  DefineAccessor(isolate, kRecord, "nextSibling", [](const CallbackInfo& info) {
    Return(info, Receiver<MutationRecord>(info)->next_sibling.Get());
  });
  DefineAccessor(
      isolate, kRecord, "attributeName", [](const CallbackInfo& info) {
        ReturnOrNull(info, Receiver<MutationRecord>(info)->attribute_name);
      });
  DefineAccessor(
      isolate, kRecord, "attributeNamespace",
      [](const CallbackInfo& info) { info.GetReturnValue().SetNull(); });
  DefineAccessor(isolate, kRecord, "oldValue", [](const CallbackInfo& info) {
    ReturnOrNull(info, Receiver<MutationRecord>(info)->old_value);
  });
}

// --- ResizeObserver / IntersectionObserver ----------------------------------

// Both observers report each target once, shortly after observe(): with a
// static viewport and no scrolling, the first observation is also the last.
class LayoutObserver final : public ScriptWrappable {
 public:
  LayoutObserver(Isolate* isolate, Interface interface,
                 Local<Function> callback)
      : ScriptWrappable(interface) {
    callback_.Reset(isolate, callback);
  }
  void Trace(Visitor* visitor) const override {
    if (DeferTraceIfConcurrent(visitor, this)) return;
    ScriptWrappable::Trace(visitor);
    visitor->Trace(callback_);
    for (const auto& target : pending_) visitor->Trace(target);
  }

  void Observe(Isolate* isolate, Element* target) {
    pending_.emplace_back(target);
    if (task_posted_) return;
    task_posted_ = true;
    cppgc::Persistent<LayoutObserver> self(this);
    PostTask(isolate, [isolate, self] { self->Deliver(isolate); });
  }
  void Unobserve(Element* target) {
    for (size_t i = 0; i < pending_.size(); ++i) {
      if (pending_[i] == target) {
        pending_.erase(pending_.begin() + i);
        return;
      }
    }
  }
  void Disconnect() { pending_.clear(); }

 private:
  void Deliver(Isolate* isolate) {
    task_posted_ = false;
    if (pending_.empty()) return;
    HandleScope handle_scope(isolate);
    Local<Function> callback = callback_.Get(isolate);
    Local<Context> context;
    if (!callback->GetCreationContext(isolate).ToLocal(&context)) return;
    Context::Scope context_scope(context);
    LocalVector<Value> entries(isolate);
    // The targets stay in pending_, where the GC sees them, until each one has
    // an entry that refers to its wrapper.
    const size_t count = pending_.size();
    for (size_t i = 0; i < count; ++i) {
      Element* target = pending_[i].Get();
      LayoutRect rect = BoundingClientRect(target);
      Local<Value> dom_rect =
          ToV8(isolate, MakeGarbageCollected<DOMRect>(isolate, rect.x, rect.y,
                                                      rect.width, rect.height));
      Local<Object> entry = Object::New(isolate);
      auto set = [&](const char* name, Local<Value> value) {
        USE(entry->Set(context, V8Name(isolate, name), value));
      };
      set("target", ToV8(isolate, target));
      if (interface() == Interface::kResizeObserver) {
        set("contentRect", dom_rect);
        Local<Object> size = Object::New(isolate);
        USE(size->Set(context, V8Name(isolate, "inlineSize"),
                      Number::New(isolate, rect.width)));
        USE(size->Set(context, V8Name(isolate, "blockSize"),
                      Number::New(isolate, rect.height)));
        Local<Value> size_value = size;
        Local<Value> sizes = Array::New(isolate, &size_value, 1);
        set("contentBoxSize", sizes);
        set("borderBoxSize", sizes);
        set("devicePixelContentBoxSize", sizes);
      } else {
        set("boundingClientRect", dom_rect);
        set("intersectionRect", dom_rect);
        set("rootBounds", Null(isolate));
        set("isIntersecting", Boolean::New(isolate, true));
        set("isVisible", Boolean::New(isolate, true));
        set("intersectionRatio", Number::New(isolate, 1));
        set("time", Number::New(isolate, MonotonicNowMs()));
      }
      entries.push_back(entry);
    }
    pending_.erase(pending_.begin(), pending_.begin() + count);
    Local<Value> self = ToV8(isolate, this);
    Local<Value> args[] = {Array::New(isolate, entries.data(), entries.size()),
                           self};
    CallAndReport(isolate, nullptr, callback, self, 2, args);
  }

  TracedReference<Function> callback_;
  MOCK_DOM_TRACED_CONTAINER
  std::vector<Member<Element>> pending_;
  bool task_posted_ = false;
};

CALLBACK(LayoutObserverConstructor) {
  if (!RequireConstructCall(info) || !RequireArguments(info, 1)) return;
  Isolate* isolate = info.GetIsolate();
  if (!info[0]->IsFunction()) {
    ThrowTypeError(isolate, "parameter 1 is not of type 'Function'.");
    return;
  }
  Interface interface = static_cast<Interface>(DataInt(info));
  WrapThis(info, MakeGarbageCollected<LayoutObserver>(
                     isolate, isolate, interface, info[0].As<Function>()));
}

void InstallLayoutObserverBindings(Isolate* isolate) {
  for (Interface interface :
       {Interface::kResizeObserver, Interface::kIntersectionObserver}) {
    SetConstructor(isolate, interface, LayoutObserverConstructor, 1);
    DefineMethod(
        isolate, interface, "observe",
        [](const CallbackInfo& info) {
          Isolate* isolate = info.GetIsolate();
          Element* target =
              info.Length() ? To<Element>(isolate, info[0], Interface::kElement)
                            : nullptr;
          if (!target) {
            ThrowTypeError(isolate, "parameter 1 is not of type 'Element'.");
            return;
          }
          Receiver<LayoutObserver>(info)->Observe(isolate, target);
        },
        1);
    DefineMethod(
        isolate, interface, "unobserve",
        [](const CallbackInfo& info) {
          Element* target =
              info.Length()
                  ? To<Element>(info.GetIsolate(), info[0], Interface::kElement)
                  : nullptr;
          if (target) Receiver<LayoutObserver>(info)->Unobserve(target);
        },
        1);
    DefineMethod(isolate, interface, "disconnect",
                 [](const CallbackInfo& info) {
                   Receiver<LayoutObserver>(info)->Disconnect();
                 });
    DefineMethod(isolate, interface, "takeRecords",
                 [](const CallbackInfo& info) {
                   info.GetReturnValue().Set(Array::New(info.GetIsolate(), 0));
                 });
  }
}

// --- matchMedia --------------------------------------------------------------

class MediaQueryList final : public EventTarget {
 public:
  MediaQueryList(String media, bool matches)
      : EventTarget(Interface::kMediaQueryList),
        media(std::move(media)),
        matches(matches) {}
  String media;
  bool matches;
};

void InstallMediaQueryBindings(Isolate* isolate) {
  const Interface kList = Interface::kMediaQueryList;
  DefineAccessor(isolate, kList, "matches", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(Receiver<MediaQueryList>(info)->matches);
  });
  DefineAccessor(isolate, kList, "media", [](const CallbackInfo& info) {
    Return(info, Receiver<MediaQueryList>(info)->media);
  });
  // Legacy addListener/removeListener are aliases for the "change" event.
  DefineMethod(
      isolate, kList, "addListener",
      [](const CallbackInfo& info) {
        if (info.Length() < 1 || !info[0]->IsObject()) return;
        Receiver<MediaQueryList>(info)->AddEventListener(
            info.GetIsolate(), AtomicString("change"), info[0], false, false,
            false);
      },
      1);
  DefineMethod(
      isolate, kList, "removeListener",
      [](const CallbackInfo& info) {
        if (info.Length() < 1 || !info[0]->IsObject()) return;
        Receiver<MediaQueryList>(info)->RemoveEventListener(
            info.GetIsolate(), AtomicString("change"), info[0], false);
      },
      1);
}

// --- AbortController ---------------------------------------------------------

class AbortController final : public ScriptWrappable {
 public:
  explicit AbortController(AbortSignal* signal)
      : ScriptWrappable(Interface::kAbortController), signal(signal) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(signal);
  }
  Member<AbortSignal> signal;
};

Local<Value> AbortError(Isolate* isolate) {
  Local<Value> error = Exception::Error(v8::String::NewFromUtf8Literal(
      isolate, "signal is aborted without reason"));
  Local<Context> context = isolate->GetCurrentContext();
  USE(error.As<Object>()->Set(context, V8Name(isolate, "name"),
                              V8Name(isolate, "AbortError")));
  return error;
}

void InstallAbortBindings(Isolate* isolate) {
  SetConstructor(
      isolate, Interface::kAbortController, [](const CallbackInfo& info) {
        if (!RequireConstructCall(info)) return;
        Isolate* isolate = info.GetIsolate();
        WrapThis(info,
                 MakeGarbageCollected<AbortController>(
                     isolate, MakeGarbageCollected<AbortSignal>(isolate)));
      });
  DefineAccessor(isolate, Interface::kAbortController, "signal",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<AbortController>(info)->signal.Get());
                 });
  DefineMethod(
      isolate, Interface::kAbortController, "abort",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        Local<Value> reason = info.Length() > 0 && !info[0]->IsUndefined()
                                  ? info[0]
                                  : AbortError(isolate);
        Receiver<AbortController>(info)->signal->Abort(isolate, reason);
      });
  DefineAccessor(
      isolate, Interface::kAbortSignal, "aborted",
      [](const CallbackInfo& info) {
        info.GetReturnValue().Set(Receiver<AbortSignal>(info)->aborted());
      });
  DefineAccessor(isolate, Interface::kAbortSignal, "reason",
                 [](const CallbackInfo& info) {
                   info.GetReturnValue().Set(
                       Receiver<AbortSignal>(info)->Reason(info.GetIsolate()));
                 });
  DefineMethod(
      isolate, Interface::kAbortSignal, "throwIfAborted",
      [](const CallbackInfo& info) {
        AbortSignal* signal = Receiver<AbortSignal>(info);
        if (signal->aborted()) {
          info.GetIsolate()->ThrowException(signal->Reason(info.GetIsolate()));
        }
      });
  auto on_abort_getter = [](const CallbackInfo& info) {
    info.GetReturnValue().Set(Receiver<AbortSignal>(info)->GetAttributeListener(
        info.GetIsolate(), AtomicString("abort")));
  };
  auto on_abort_setter = [](const CallbackInfo& info) {
    Receiver<AbortSignal>(info)->SetAttributeListener(
        info.GetIsolate(), AtomicString("abort"), info[0]);
  };
  DefineAccessor(isolate, Interface::kAbortSignal, "onabort", on_abort_getter,
                 on_abort_setter);
}

// --- MessageChannel ----------------------------------------------------------

class MessagePort final : public EventTarget {
 public:
  MessagePort() : EventTarget(Interface::kMessagePort) {}
  void Trace(Visitor* visitor) const override {
    EventTarget::Trace(visitor);
    visitor->Trace(entangled);
  }
  // Messages are delivered as tasks, in order. The payload is passed by
  // reference rather than structured-cloned.
  void PostMessage(Isolate* isolate, Local<Value> data) {
    MessagePort* target = entangled.Get();
    if (!target || closed) return;
    cppgc::Persistent<MessagePort> receiver(target);
    auto payload = std::make_shared<Global<Value>>(isolate, data);
    PostTask(isolate, [isolate, receiver, payload] {
      if (receiver->closed) return;
      HandleScope handle_scope(isolate);
      Event* event = MakeGarbageCollected<Event>(
          isolate, isolate, Interface::kMessageEvent, AtomicString("message"));
      event->any_slots[event->FieldIndex("data")].Reset(isolate,
                                                        payload->Get(isolate));
      event->is_trusted = true;
      receiver->DispatchEvent(isolate, event);
    });
  }
  Member<MessagePort> entangled;
  bool closed = false;
};

class MessageChannel final : public ScriptWrappable {
 public:
  explicit MessageChannel(Isolate* isolate)
      : ScriptWrappable(Interface::kMessageChannel),
        port1(MakeGarbageCollected<MessagePort>(isolate)),
        port2(MakeGarbageCollected<MessagePort>(isolate)) {
    port1->entangled = port2;
    port2->entangled = port1;
  }
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(port1);
    visitor->Trace(port2);
  }
  Member<MessagePort> port1;
  Member<MessagePort> port2;
};

void InstallMessageBindings(Isolate* isolate) {
  SetConstructor(isolate, Interface::kMessageChannel,
                 [](const CallbackInfo& info) {
                   if (!RequireConstructCall(info)) return;
                   WrapThis(info, MakeGarbageCollected<MessageChannel>(
                                      info.GetIsolate(), info.GetIsolate()));
                 });
  DefineAccessor(isolate, Interface::kMessageChannel, "port1",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<MessageChannel>(info)->port1.Get());
                 });
  DefineAccessor(isolate, Interface::kMessageChannel, "port2",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<MessageChannel>(info)->port2.Get());
                 });
  const Interface kPort = Interface::kMessagePort;
  DefineMethod(
      isolate, kPort, "postMessage",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        Local<Value> data =
            info.Length() > 0 ? info[0] : Undefined(isolate).As<Value>();
        Receiver<MessagePort>(info)->PostMessage(isolate, data);
      },
      1);
  DefineMethod(isolate, kPort, "start", [](const CallbackInfo& info) {});
  DefineMethod(isolate, kPort, "close", [](const CallbackInfo& info) {
    Receiver<MessagePort>(info)->closed = true;
  });
  DefineAccessor(
      isolate, kPort, "onmessage",
      [](const CallbackInfo& info) {
        info.GetReturnValue().Set(
            Receiver<MessagePort>(info)->GetAttributeListener(
                info.GetIsolate(), AtomicString("message")));
      },
      [](const CallbackInfo& info) {
        Receiver<MessagePort>(info)->SetAttributeListener(
            info.GetIsolate(), AtomicString("message"), info[0]);
      });
}

// --- Window members ----------------------------------------------------------

const char* const kWindowEventHandlers[] = {
    "abort",
    "afterprint",
    "beforeprint",
    "beforeunload",
    "blur",
    "change",
    "click",
    "contextmenu",
    "dblclick",
    "error",
    "focus",
    "hashchange",
    "input",
    "keydown",
    "keypress",
    "keyup",
    "languagechange",
    "load",
    "message",
    "messageerror",
    "mousedown",
    "mousemove",
    "mouseout",
    "mouseover",
    "mouseup",
    "offline",
    "online",
    "pagehide",
    "pageshow",
    "pointerdown",
    "pointermove",
    "pointerup",
    "popstate",
    "rejectionhandled",
    "resize",
    "scroll",
    "storage",
    "submit",
    "touchend",
    "touchmove",
    "touchstart",
    "transitionend",
    "unhandledrejection",
    "unload",
    "wheel",
    "DOMContentLoaded",
};

CALLBACK(WindowEventHandlerGetter) {
  Isolate* isolate = info.GetIsolate();
  info.GetReturnValue().Set(CurrentWindow(info)->GetAttributeListener(
      isolate, ToAtomicString(isolate, info.Data().As<v8::String>())));
}

CALLBACK(WindowEventHandlerSetter) {
  Isolate* isolate = info.GetIsolate();
  CurrentWindow(info)->SetAttributeListener(
      isolate, ToAtomicString(isolate, info.Data().As<v8::String>()), info[0]);
}

CALLBACK(WindowSelf) { Return(info, CurrentWindow(info)); }

// The window that embeds |window| in a frame, or |window| itself if it is
// not in a frame.
Window* ParentWindow(Isolate* isolate, Window* window) {
  if (window->frame_element.IsEmpty()) return window;
  Element* frame = To<Element>(isolate, window->frame_element.Get(isolate),
                               Interface::kElement);
  Window* parent = frame ? frame->WrapperWindow() : nullptr;
  return parent ? parent : window;
}

CALLBACK(WindowParent) {
  Return(info, ParentWindow(info.GetIsolate(), CurrentWindow(info)));
}

CALLBACK(WindowTop) {
  Isolate* isolate = info.GetIsolate();
  Window* window = CurrentWindow(info);
  while (true) {
    Window* parent = ParentWindow(isolate, window);
    if (parent == window) break;
    window = parent;
  }
  Return(info, window);
}

CALLBACK(WindowGetComputedStyle) {
  Isolate* isolate = info.GetIsolate();
  Element* element = info.Length()
                         ? To<Element>(isolate, info[0], Interface::kElement)
                         : nullptr;
  if (!element) {
    ThrowTypeError(isolate,
                   "Failed to execute 'getComputedStyle' on 'Window': "
                   "parameter 1 is not of type 'Element'.");
    return;
  }
  Return(info,
         MakeGarbageCollected<CSSStyleDeclaration>(isolate, element, true));
}

CALLBACK(WindowMatchMedia) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  String query;
  if (!ToString(info, info[0], &query)) return;
  Return(info, MakeGarbageCollected<MediaQueryList>(
                   isolate, query,
                   EvaluateMediaQuery(query.ToUtf8(), CurrentWindow(info))));
}

CALLBACK(WindowQueueMicrotask) {
  Isolate* isolate = info.GetIsolate();
  if (info.Length() < 1 || !info[0]->IsFunction()) {
    ThrowTypeError(isolate, "parameter 1 is not of type 'Function'.");
    return;
  }
  isolate->GetCurrentContext()->GetMicrotaskQueue()->EnqueueMicrotask(
      isolate, info[0].As<Function>());
}

CALLBACK(WindowReportError) {
  Isolate* isolate = info.GetIsolate();
  Local<Value> error = info.Length() ? info[0] : Undefined(isolate).As<Value>();
  CurrentWindow(info)->ReportException(
      isolate, error, Exception::CreateMessage(isolate, error));
}

CALLBACK(WindowPostMessage) {
  Isolate* isolate = info.GetIsolate();
  // The receiver is the window the message goes to; the sender is the
  // window of the script that made the call.
  Window* window = CurrentWindow(info);
  cppgc::Persistent<Window> target(window);
  Local<Context> incumbent = isolate->GetIncumbentContext();
  Window* sender = incumbent.IsEmpty() ? nullptr : Window::From(incumbent);
  cppgc::Persistent<Window> source(sender ? sender : window);
  auto payload = std::make_shared<Global<Value>>(
      isolate, info.Length() ? info[0] : Undefined(isolate).As<Value>());
  PostTask(isolate, [isolate, target, source, payload] {
    HandleScope handle_scope(isolate);
    Event* event = MakeGarbageCollected<Event>(
        isolate, isolate, Interface::kMessageEvent, AtomicString("message"));
    event->any_slots[event->FieldIndex("data")].Reset(isolate,
                                                      payload->Get(isolate));
    event->set_string("origin",
                      String::FromUtf8(source->location()->url().Origin()));
    event->any_slots[event->FieldIndex("source")].Reset(
        isolate, ToV8(isolate, source.Get()));
    event->is_trusted = true;
    target->DispatchEvent(isolate, event);
  });
}

CALLBACK(WindowRequestIdleCallback) {
  Isolate* isolate = info.GetIsolate();
  if (info.Length() < 1 || !info[0]->IsFunction()) {
    ThrowTypeError(isolate, "parameter 1 is not of type 'Function'.");
    return;
  }
  static int next_handle = 1;
  int handle = next_handle++;
  cppgc::Persistent<Window> window(CurrentWindow(info));
  auto callback =
      std::make_shared<Global<Function>>(isolate, info[0].As<Function>());
  PostTask(isolate, [isolate, window, callback] {
    HandleScope handle_scope(isolate);
    Local<Context> context = window->context(isolate);
    Context::Scope context_scope(context);
    Local<Object> deadline = Object::New(isolate);
    USE(deadline->Set(context, V8Name(isolate, "didTimeout"),
                      Boolean::New(isolate, false)));
    USE(deadline->Set(context, V8Name(isolate, "timeRemaining"),
                      Function::New(context, [](const CallbackInfo& info) {
                        info.GetReturnValue().Set(50);
                      }).ToLocalChecked()));
    Local<Value> args[] = {deadline};
    CallAndReport(isolate, window.Get(), callback->Get(isolate),
                  Undefined(isolate), 1, args);
  });
  info.GetReturnValue().Set(handle);
}

std::string Base64Encode(const std::string& input) {
  static const char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  size_t i = 0;
  while (i + 2 < input.size()) {
    uint32_t n = (static_cast<unsigned char>(input[i]) << 16) |
                 (static_cast<unsigned char>(input[i + 1]) << 8) |
                 static_cast<unsigned char>(input[i + 2]);
    out.push_back(kAlphabet[n >> 18]);
    out.push_back(kAlphabet[(n >> 12) & 63]);
    out.push_back(kAlphabet[(n >> 6) & 63]);
    out.push_back(kAlphabet[n & 63]);
    i += 3;
  }
  if (i + 1 == input.size()) {
    uint32_t n = static_cast<unsigned char>(input[i]) << 16;
    out.push_back(kAlphabet[n >> 18]);
    out.push_back(kAlphabet[(n >> 12) & 63]);
    out.append("==");
  } else if (i + 2 == input.size()) {
    uint32_t n = (static_cast<unsigned char>(input[i]) << 16) |
                 (static_cast<unsigned char>(input[i + 1]) << 8);
    out.push_back(kAlphabet[n >> 18]);
    out.push_back(kAlphabet[(n >> 12) & 63]);
    out.push_back(kAlphabet[(n >> 6) & 63]);
    out.push_back('=');
  }
  return out;
}

bool Base64Decode(const std::string& input, std::string* out) {
  auto value = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  };
  uint32_t buffer = 0;
  int bits = 0;
  for (char c : input) {
    if (IsHtmlSpace(c) || c == '=') continue;
    int v = value(c);
    if (v < 0) return false;
    buffer = (buffer << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out->push_back(static_cast<char>((buffer >> bits) & 0xFF));
    }
  }
  return true;
}

// btoa/atob work on "binary strings": one Latin-1 code unit per byte.
bool ToBinaryString(const CallbackInfo& info, std::string* out) {
  String string;
  if (!ToString(info, info[0], &string)) return false;
  if (!string.is_8bit()) {
    ThrowDOMException(info.GetIsolate(), "InvalidCharacterError",
                      "The string to be encoded contains characters outside "
                      "of the Latin1 range.");
    return false;
  }
  *out = std::string(string.view8());
  return true;
}

CALLBACK(CryptoGetRandomValues) {
  Isolate* isolate = info.GetIsolate();
  if (info.Length() < 1 || !info[0]->IsArrayBufferView()) {
    ThrowTypeError(isolate, "parameter 1 is not of type 'ArrayBufferView'.");
    return;
  }
  Local<ArrayBufferView> view = info[0].As<ArrayBufferView>();
  static base::RandomNumberGenerator* rng = new base::RandomNumberGenerator();
  std::vector<uint8_t> bytes(view->ByteLength());
  rng->NextBytes(bytes.data(), bytes.size());
  uint8_t* data =
      static_cast<uint8_t*>(view->GetBuffer()->GetBackingStore()->Data()) +
      view->ByteOffset();
  memcpy(data, bytes.data(), bytes.size());
  info.GetReturnValue().Set(info[0]);
}

CALLBACK(CryptoRandomUUID) {
  static base::RandomNumberGenerator* rng = new base::RandomNumberGenerator();
  uint8_t bytes[16];
  rng->NextBytes(bytes, sizeof(bytes));
  bytes[6] = (bytes[6] & 0x0F) | 0x40;
  bytes[8] = (bytes[8] & 0x3F) | 0x80;
  char buffer[37];
  snprintf(buffer, sizeof(buffer),
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x"
           "%02x",
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
           bytes[7], bytes[8], bytes[9], bytes[10], bytes[11], bytes[12],
           bytes[13], bytes[14], bytes[15]);
  info.GetReturnValue().Set(ToV8StringFromUtf8(info.GetIsolate(), buffer));
}

// new Image(width, height) and new Option(text, value).
CALLBACK(ImageConstructor) {
  if (!RequireConstructCall(info)) return;
  Isolate* isolate = info.GetIsolate();
  Element* image = CurrentWindow(info)->document()->CreateElement(
      isolate, Namespace::kHTML, AtomicString("img"));
  for (int i = 0; i < 2 && i < info.Length(); ++i) {
    AtomicString value;
    if (!ToAtom(info, info[i], &value)) return;
    image->SetAttribute(isolate, AtomicString(i == 0 ? "width" : "height"),
                        value);
  }
  Return(info, image);
}

CALLBACK(OptionConstructor) {
  if (!RequireConstructCall(info)) return;
  Isolate* isolate = info.GetIsolate();
  Element* option = CurrentWindow(info)->document()->CreateElement(
      isolate, Namespace::kHTML, AtomicString("option"));
  if (info.Length() > 0 && !info[0]->IsUndefined()) {
    String text;
    if (!ToString(info, info[0], &text)) return;
    option->SetTextContent(isolate, text);
  }
  if (info.Length() > 1 && !info[1]->IsUndefined()) {
    AtomicString value;
    if (!ToAtom(info, info[1], &value)) return;
    option->SetAttribute(isolate, AtomicString("value"), value);
  }
  Return(info, option);
}

void InstallWindowMembers(Isolate* isolate) {
  for (const char* name : {"window", "self", "frames"}) {
    DefineGlobalAccessor(isolate, name, WindowSelf);
  }
  DefineGlobalAccessor(isolate, "parent", WindowParent);
  DefineGlobalAccessor(isolate, "top", WindowTop);
  DefineGlobalAccessor(isolate, "document", [](const CallbackInfo& info) {
    Return(info, CurrentWindow(info)->document());
  });
  DefineGlobalAccessor(
      isolate, "location",
      [](const CallbackInfo& info) {
        Return(info, CurrentWindow(info)->location());
      },
      [](const CallbackInfo& info) {
        std::string url;
        if (!ToStdString(info, info[0], &url)) return;
        CurrentWindow(info)->location()->Navigate(info.GetIsolate(), url);
      });
  DefineGlobalAccessor(isolate, "history", [](const CallbackInfo& info) {
    Return(info, CurrentWindow(info)->history());
  });
  DefineGlobalAccessor(isolate, "localStorage", [](const CallbackInfo& info) {
    Return(info, CurrentWindow(info)->local_storage());
  });
  DefineGlobalAccessor(isolate, "sessionStorage", [](const CallbackInfo& info) {
    Return(info, CurrentWindow(info)->session_storage());
  });
  DefineGlobalAccessor(isolate, "customElements", [](const CallbackInfo& info) {
    Return(info, CurrentWindow(info)->custom_elements());
  });
  DefineGlobalAccessor(isolate, "frameElement", [](const CallbackInfo& info) {
    Window* window = CurrentWindow(info);
    if (window->frame_element.IsEmpty()) {
      info.GetReturnValue().SetNull();
    } else {
      info.GetReturnValue().Set(window->frame_element.Get(info.GetIsolate()));
    }
  });
  DefineGlobalAccessor(isolate, "origin", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(ToV8StringFromUtf8(
        info.GetIsolate(), CurrentWindow(info)->location()->url().Origin()));
  });

  struct Number {
    const char* name;
    double (*get)(Window*);
  };
  static const Number kNumbers[] = {
      {"innerWidth", [](Window* w) -> double { return w->inner_width; }},
      {"innerHeight", [](Window* w) -> double { return w->inner_height; }},
      {"outerWidth", [](Window* w) -> double { return w->inner_width; }},
      {"outerHeight", [](Window* w) -> double { return w->inner_height; }},
      {"devicePixelRatio", [](Window*) -> double { return 1; }},
      {"scrollX", [](Window*) -> double { return 0; }},
      {"scrollY", [](Window*) -> double { return 0; }},
      {"pageXOffset", [](Window*) -> double { return 0; }},
      {"pageYOffset", [](Window*) -> double { return 0; }},
      {"screenX", [](Window*) -> double { return 0; }},
      {"screenY", [](Window*) -> double { return 0; }},
      {"screenLeft", [](Window*) -> double { return 0; }},
      {"screenTop", [](Window*) -> double { return 0; }},
      {"length", [](Window*) -> double { return 0; }},
  };
  for (size_t i = 0; i < arraysize(kNumbers); ++i) {
    DefineGlobalAccessor(
        isolate, kNumbers[i].name,
        [](const CallbackInfo& info) {
          info.GetReturnValue().Set(
              kNumbers[DataInt(info)].get(CurrentWindow(info)));
        },
        nullptr, Integer::New(isolate, static_cast<int>(i)));
  }
  DefineGlobalAccessor(
      isolate, "isSecureContext",
      [](const CallbackInfo& info) { info.GetReturnValue().Set(true); });
  DefineGlobalAccessor(isolate, "closed", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(false);
  });
  DefineGlobalAccessor(isolate, "opener", [](const CallbackInfo& info) {
    info.GetReturnValue().SetNull();
  });

  for (const char* type : kWindowEventHandlers) {
    if (strcmp(type, "DOMContentLoaded") == 0) continue;
    // d8 defines its own `onerror` global; ours replaces it.
    std::string property = std::string("on") + type;
    DefineGlobalAccessor(isolate, property.c_str(), WindowEventHandlerGetter,
                         WindowEventHandlerSetter, V8Name(isolate, type));
  }

  DefineGlobalMethod(isolate, "getComputedStyle", WindowGetComputedStyle, 1);
  DefineGlobalMethod(isolate, "matchMedia", WindowMatchMedia, 1);
  DefineGlobalMethod(isolate, "queueMicrotask", WindowQueueMicrotask, 1);
  DefineGlobalMethod(isolate, "reportError", WindowReportError, 1);
  DefineGlobalMethod(isolate, "postMessage", WindowPostMessage, 1);
  DefineGlobalMethod(isolate, "requestIdleCallback", WindowRequestIdleCallback,
                     1);
  DefineGlobalMethod(
      isolate, "cancelIdleCallback",
      [](const CallbackInfo& info) {
        // Idle callbacks run on the next task; cancelling one
        // that is already queued is not supported.
      },
      1);
  // No viewport to scroll, no focus to move between windows.
  for (const char* name :
       {"scrollTo", "scroll", "scrollBy", "focus", "blur", "stop"}) {
    DefineGlobalMethod(isolate, name, [](const CallbackInfo& info) {});
  }
  // window.print is left alone: d8's own print() lives under that name.
  for (const char* name : {"alert", "confirm", "prompt", "open"}) {
    DefineGlobalMethod(isolate, name, [](const CallbackInfo& info) {
      CurrentWindow(info)->ReportUnsupported(
          info.GetIsolate(), "window.alert/confirm/prompt/open");
    });
  }
  DefineGlobalMethod(
      isolate, "btoa",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        std::string binary;
        if (!ToBinaryString(info, &binary)) return;
        info.GetReturnValue().Set(
            ToV8StringFromUtf8(info.GetIsolate(), Base64Encode(binary)));
      },
      1);
  DefineGlobalMethod(
      isolate, "atob",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        std::string encoded, decoded;
        if (!ToBinaryString(info, &encoded)) return;
        if (!Base64Decode(encoded, &decoded)) {
          ThrowDOMException(info.GetIsolate(), "InvalidCharacterError",
                            "The string to be decoded is not correctly "
                            "encoded.");
          return;
        }
        Return(info, String::FromLatin1(decoded));
      },
      1);

  Local<ObjectTemplate> global =
      InterfaceTemplate(isolate, Interface::kWindow)->InstanceTemplate();
  global->Set(V8Name(isolate, "Image"),
              FunctionTemplate::New(isolate, ImageConstructor));
  global->Set(V8Name(isolate, "Option"),
              FunctionTemplate::New(isolate, OptionConstructor));

  // Interface objects. Templates set on the global template are instantiated
  // together with each global object.
  for (int i = 0; i < kInterfaceCount; ++i) {
    Interface interface = static_cast<Interface>(i);
    if (interface == Interface::kHTMLAllCollection) continue;
    global->Set(V8Name(isolate, InterfaceName(interface)),
                InterfaceTemplate(isolate, interface), DontEnum);
  }
  // Instantiating HTMLAllCollection invalidates a V8 protector (see
  // InstallCollectionBindings), so that has to wait until a page asks for it.
  global->SetLazyDataProperty(
      V8Name(isolate, "HTMLAllCollection"),
      [](Local<v8::Name>, const PropertyCallbackInfo<Value>& info) {
        Isolate* isolate = info.GetIsolate();
        Local<v8::Function> function;
        if (InterfaceTemplate(isolate, Interface::kHTMLAllCollection)
                ->GetFunction(isolate->GetCurrentContext())
                .ToLocal(&function)) {
          info.GetReturnValue().Set(function);
        }
      },
      Local<Value>(), DontEnum);
  global->Set(V8Name(isolate, "HTMLDocument"),
              InterfaceTemplate(isolate, Interface::kDocument), DontEnum);
  global->Set(V8Name(isolate, "DOMRectReadOnly"),
              InterfaceTemplate(isolate, Interface::kDOMRect), DontEnum);
  global->Set(V8Name(isolate, "WebKitMutationObserver"),
              InterfaceTemplate(isolate, Interface::kMutationObserver),
              DontEnum);
}

// --- d8.mockDom: the page loader's back door --------------------------------

Window* WindowArgument(const CallbackInfo& info, int index) {
  Window* window =
      info.Length() > index
          ? To<Window>(info.GetIsolate(), info[index], Interface::kWindow)
          : nullptr;
  if (!window) {
    ThrowTypeError(info.GetIsolate(),
                   "d8.mockDom: expected a window (a realm's global object).");
  }
  return window;
}

CALLBACK(TestingParseDocument) {
  Isolate* isolate = info.GetIsolate();
  Window* window = WindowArgument(info, 0);
  std::string html;
  if (!window || info.Length() < 2 || !ToStdString(info, info[1], &html)) {
    return;
  }
  Document* document = window->document();
  document->RemoveChildren(isolate);
  ParseHTMLDocument(isolate, document, html);
}

CALLBACK(TestingSetURL) {
  Window* window = WindowArgument(info, 0);
  std::string url;
  if (!window || info.Length() < 2 || !ToStdString(info, info[1], &url)) {
    return;
  }
  ParsedURL parsed = ParsedURL::Parse(url);
  if (!parsed.valid) {
    ThrowTypeError(info.GetIsolate(), "d8.mockDom.setURL: invalid URL " + url);
    return;
  }
  window->location()->SetURLWithoutNavigation(parsed);
}

CALLBACK(TestingSetReadyState) {
  Isolate* isolate = info.GetIsolate();
  Window* window = WindowArgument(info, 0);
  AtomicString state;
  if (!window || info.Length() < 2 || !ToAtom(info, info[1], &state)) return;
  Document* document = window->document();
  document->ready_state = state;
  Event* event = MakeGarbageCollected<Event>(
      isolate, isolate, Interface::kEvent, AtomicString("readystatechange"));
  event->is_trusted = true;
  document->DispatchEvent(isolate, event);
}

CALLBACK(TestingSetResourceCallback) {
  Isolate* isolate = info.GetIsolate();
  Window* window = WindowArgument(info, 0);
  if (!window) return;
  if (info.Length() > 1 && info[1]->IsFunction()) {
    window->resource_callback.Reset(isolate, info[1].As<Function>());
  } else {
    window->resource_callback.Reset();
  }
}

CALLBACK(TestingSetFrameWindow) {
  Isolate* isolate = info.GetIsolate();
  Element* frame = info.Length()
                       ? To<Element>(isolate, info[0], Interface::kElement)
                       : nullptr;
  if (!frame) {
    ThrowTypeError(isolate, "d8.mockDom.setFrameWindow: expected an element.");
    return;
  }
  Window* window = info.Length() > 1 && !info[1]->IsNullOrUndefined()
                       ? WindowArgument(info, 1)
                       : nullptr;
  frame->EnsureRareData(isolate)->content_window = window;
  if (window) window->frame_element.Reset(isolate, info[0]);
}

CALLBACK(TestingSetCurrentScript) {
  Isolate* isolate = info.GetIsolate();
  Window* window = WindowArgument(info, 0);
  if (!window) return;
  window->document()->current_script =
      info.Length() > 1 ? To<Element>(isolate, info[1], Interface::kElement)
                        : nullptr;
}

CALLBACK(TestingSetViewport) {
  Window* window = WindowArgument(info, 0);
  int width, height;
  if (!window || info.Length() < 3 || !ToInt(info, info[1], &width) ||
      !ToInt(info, info[2], &height)) {
    return;
  }
  window->inner_width = width;
  window->inner_height = height;
  window->document()->InvalidateLayout();
  InvalidateAllStyles(window->document());
}

CALLBACK(TestingSetStyleSheetText) {
  Isolate* isolate = info.GetIsolate();
  Element* link = info.Length()
                      ? To<Element>(isolate, info[0], Interface::kElement)
                      : nullptr;
  String text;
  if (!link || info.Length() < 2 || !ToString(info, info[1], &text)) {
    ThrowTypeError(isolate,
                   "d8.mockDom.setStyleSheetText: expected (link, text).");
    return;
  }
  link->EnsureRareData(isolate)->link_sheet_text = text;
  link->document()->InvalidateLayout();
  InvalidateAllStyles(link->document());
}

// What String.prototype.trim() strips.
bool IsJSWhiteSpace(uint16_t c) {
  return (c >= 0x09 && c <= 0x0D) || c == 0x20 || c == 0xA0 || c == 0x1680 ||
         (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 ||
         c == 0x202F || c == 0x205F || c == 0x3000 || c == 0xFEFF;
}

struct Fingerprint {
  double elements = 0;
  double text_nodes = 0;
  double text_length = 0;
  double attributes = 0;
  std::map<std::string, double> tags;
};

void AddToFingerprint(Node* parent, Fingerprint* out) {
  for (Node* child = parent->first_child(); child;
       child = child->next_sibling()) {
    if (child->IsElement()) {
      Element* element = static_cast<Element*>(child);
      element->SynchronizeAttributes();
      ++out->elements;
      out->attributes += static_cast<double>(element->attributes().size());
      ++out->tags[element->local_name().ToUtf8()];
      ShadowRoot* shadow = element->shadow_root();
      if (shadow && shadow->open()) AddToFingerprint(shadow, out);
      AddToFingerprint(element, out);
    } else if (child->IsText()) {
      StringImpl* data = static_cast<CharacterData*>(child)->data().impl();
      uint32_t begin = 0;
      uint32_t end = data ? data->length() : 0;
      while (begin < end && IsJSWhiteSpace(data->at(begin))) ++begin;
      while (end > begin && IsJSWhiteSpace(data->at(end - 1))) --end;
      if (end > begin) {
        ++out->text_nodes;
        out->text_length += end - begin;
      }
    }
  }
}

// d8.mockDom.fingerprint(window): a structural summary of the document's
// body, computed without creating wrappers. It mirrors domFingerprint() in
// experimental/d8/speedometer-harness/dom-fingerprint.js, which
// produces the reference values in a real browser.
CALLBACK(TestingFingerprint) {
  Isolate* isolate = info.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Window* window = WindowArgument(info, 0);
  if (!window) return;
  Fingerprint fingerprint;
  if (Element* body = window->document()->body()) {
    AddToFingerprint(body, &fingerprint);
  }
  Local<Object> result = Object::New(isolate);
  auto set = [&](Local<Object> object, const std::string& name, double value) {
    USE(object->Set(context, ToV8StringFromUtf8(isolate, name),
                    Number::New(isolate, value)));
  };
  set(result, "elements", fingerprint.elements);
  set(result, "textNodes", fingerprint.text_nodes);
  set(result, "textLength", fingerprint.text_length);
  set(result, "attributes", fingerprint.attributes);
  Local<Object> tags = Object::New(isolate);
  for (const auto& tag : fingerprint.tags) set(tags, tag.first, tag.second);
  USE(result->Set(context, V8Name(isolate, "tags"), tags));
  info.GetReturnValue().Set(result);
}

CALLBACK(TestingDetachWindow) {
  Window* window = WindowArgument(info, 0);
  if (window) window->detached = true;
}

CALLBACK(TestingUncaughtErrorCount) {
  info.GetReturnValue().Set(UncaughtErrorCount());
}

#undef CALLBACK

}  // namespace

Local<ObjectTemplate> CreateTestingTemplate(Isolate* isolate) {
  Local<ObjectTemplate> testing = ObjectTemplate::New(isolate);
  auto add = [&](const char* name, FunctionCallback callback) {
    testing->Set(V8Name(isolate, name),
                 FunctionTemplate::New(isolate, callback));
  };
  add("parseDocument", TestingParseDocument);
  add("setURL", TestingSetURL);
  add("setReadyState", TestingSetReadyState);
  add("setResourceCallback", TestingSetResourceCallback);
  add("setFrameWindow", TestingSetFrameWindow);
  add("setCurrentScript", TestingSetCurrentScript);
  add("setViewport", TestingSetViewport);
  add("detachWindow", TestingDetachWindow);
  add("fingerprint", TestingFingerprint);
  add("setStyleSheetText", TestingSetStyleSheetText);
  add("uncaughtErrorCount", TestingUncaughtErrorCount);
  return testing;
}

void InstallWindowBindings(Isolate* isolate) {
  InstallLocationBindings(isolate);
  InstallHistoryBindings(isolate);
  InstallStorageBindings(isolate);
  InstallCustomElementBindings(isolate);
  InstallMutationObserverBindings(isolate);
  InstallLayoutObserverBindings(isolate);
  InstallMediaQueryBindings(isolate);
  InstallAbortBindings(isolate);
  InstallMessageBindings(isolate);
  InstallWindowMembers(isolate);
}

void InitializeRealmForWindow(Isolate* isolate, Local<Context> context) {
  Local<Object> global = context->Global();
  auto object = [&](std::initializer_list<std::pair<const char*, Local<Value>>>
                        properties) {
    Local<Object> result = Object::New(isolate);
    for (const auto& property : properties) {
      USE(result->Set(context, V8Name(isolate, property.first),
                      property.second));
    }
    return result;
  };
  auto string = [&](const char* value) -> Local<Value> {
    return V8Name(isolate, value);
  };
  auto number = [&](double value) -> Local<Value> {
    return Number::New(isolate, value);
  };
  auto function = [&](FunctionCallback callback) -> Local<Value> {
    return Function::New(context, callback).ToLocalChecked();
  };

  Local<Value> language = string("en-US");
  Local<Object> navigator = object({
      {"userAgent",
       string("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, "
              "like Gecko) d8-mock-dom Safari/537.36")},
      {"appName", string("Netscape")},
      {"appVersion", string("5.0 (X11; Linux x86_64)")},
      {"platform", string("Linux x86_64")},
      {"vendor", string("Google Inc.")},
      {"product", string("Gecko")},
      {"language", language},
      {"languages", Array::New(isolate, &language, 1)},
      {"onLine", Boolean::New(isolate, true)},
      {"cookieEnabled", Boolean::New(isolate, true)},
      {"hardwareConcurrency", number(8)},
      {"maxTouchPoints", number(0)},
      {"webdriver", Boolean::New(isolate, false)},
  });
  USE(global->Set(context, V8Name(isolate, "navigator"), navigator));
  USE(global->Set(context, V8Name(isolate, "clientInformation"), navigator));

  Window* window = Window::From(context);
  USE(global->Set(context, V8Name(isolate, "screen"),
                  object({{"width", number(window->inner_width)},
                          {"height", number(window->inner_height)},
                          {"availWidth", number(window->inner_width)},
                          {"availHeight", number(window->inner_height)},
                          {"colorDepth", number(24)},
                          {"pixelDepth", number(24)}})));
  USE(global->Set(context, V8Name(isolate, "crypto"),
                  object({{"getRandomValues", function(CryptoGetRandomValues)},
                          {"randomUUID", function(CryptoRandomUUID)}})));
  USE(global->Set(
      context, V8Name(isolate, "CSS"),
      object({{"supports", function([](const CallbackInfo& info) {
                 // No style engine: nothing is "supported".
                 info.GetReturnValue().Set(false);
               })},
              {"escape", function([](const CallbackInfo& info) {
                 std::string ident;
                 if (info.Length() < 1 || !ToStdString(info, info[0], &ident)) {
                   return;
                 }
                 std::string out;
                 for (size_t i = 0; i < ident.size(); ++i) {
                   unsigned char c = static_cast<unsigned char>(ident[i]);
                   bool plain = c >= 0x80 || c == '-' || c == '_' ||
                                (c >= 'a' && c <= 'z') ||
                                (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9' && i > 0);
                   if (!plain) out.push_back('\\');
                   out.push_back(ident[i]);
                 }
                 info.GetReturnValue().Set(
                     ToV8StringFromUtf8(info.GetIsolate(), out));
               })}})));
}

}  // namespace mock_dom
}  // namespace v8
