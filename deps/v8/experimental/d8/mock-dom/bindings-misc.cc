// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bindings for URL, URLSearchParams, DOMParser and XMLSerializer.

#include <algorithm>

#include "experimental/d8/mock-dom/bindings.h"
#include "experimental/d8/mock-dom/css.h"
#include "experimental/d8/mock-dom/html.h"
#include "experimental/d8/mock-dom/isolate-data.h"
#include "experimental/d8/mock-dom/objects.h"
#include "experimental/d8/mock-dom/url.h"
#include "include/v8-container.h"
#include "include/v8-primitive.h"
#include "include/v8-promise.h"

namespace v8 {
namespace mock_dom {

namespace {

#define CALLBACK(name) void name(const CallbackInfo& info)

void WrapThis(const CallbackInfo& info, ScriptWrappable* object) {
  Object::Wrap<kWrapperTag>(info.GetIsolate(), info.This(), object);
  object->set_wrapper(info.GetIsolate(), info.This());
}

// --- URL and URLSearchParams -------------------------------------------------

class URLObject;

class URLSearchParams final : public ScriptWrappable {
 public:
  URLSearchParams() : ScriptWrappable(Interface::kURLSearchParams) {}
  void Trace(Visitor* visitor) const override;

  // Writes the list back to the URL this object belongs to, if any.
  void Update();

  std::vector<std::pair<std::string, std::string>> pairs;
  Member<URLObject> url;
};

class URLObject final : public ScriptWrappable {
 public:
  explicit URLObject(ParsedURL url)
      : ScriptWrappable(Interface::kURL), url(std::move(url)) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(search_params);
  }
  void QueryChanged() {
    if (search_params) search_params->pairs = ParseQuery(url.query);
  }
  ParsedURL url;
  Member<URLSearchParams> search_params;
};

void URLSearchParams::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(url);
}

void URLSearchParams::Update() {
  if (url) url->url.query = SerializeQuery(pairs);
}

CALLBACK(URLConstructor) {
  if (!RequireConstructCall(info) || !RequireArguments(info, 1)) return;
  Isolate* isolate = info.GetIsolate();
  std::string input, base;
  if (!ToStdString(info, info[0], &input)) return;
  ParsedURL parsed;
  if (info.Length() > 1 && !info[1]->IsUndefined()) {
    if (!ToStdString(info, info[1], &base)) return;
    ParsedURL parsed_base = ParsedURL::Parse(base);
    if (parsed_base.valid) parsed = ParsedURL::Resolve(input, parsed_base);
  } else {
    parsed = ParsedURL::Parse(input);
  }
  if (!parsed.valid) {
    ThrowTypeError(isolate,
                   "Failed to construct 'URL': Invalid URL '" + input + "'");
    return;
  }
  WrapThis(info, MakeGarbageCollected<URLObject>(isolate, parsed));
}

struct URLPart {
  const char* name;
  std::string (*get)(const ParsedURL&);
  void (*set)(ParsedURL*, const std::string&);
};

std::string StripLeading(const std::string& value, char prefix) {
  return !value.empty() && value[0] == prefix ? value.substr(1) : value;
}

const URLPart kURLParts[] = {
    {"href", [](const ParsedURL& u) { return u.Href(); },
     [](ParsedURL* u, const std::string& v) {
       ParsedURL parsed = ParsedURL::Parse(v);
       if (parsed.valid) *u = parsed;
     }},
    {"origin", [](const ParsedURL& u) { return u.Origin(); }, nullptr},
    {"protocol", [](const ParsedURL& u) { return u.Protocol(); },
     [](ParsedURL* u, const std::string& v) {
       u->scheme = v.substr(0, v.find(':'));
     }},
    {"username", [](const ParsedURL& u) { return u.username; },
     [](ParsedURL* u, const std::string& v) { u->username = v; }},
    {"password", [](const ParsedURL& u) { return u.password; },
     [](ParsedURL* u, const std::string& v) { u->password = v; }},
    {"host", [](const ParsedURL& u) { return u.Host(); },
     [](ParsedURL* u, const std::string& v) {
       size_t colon = v.rfind(':');
       u->hostname = v.substr(0, colon);
       u->port = colon == std::string::npos ? "" : v.substr(colon + 1);
     }},
    {"hostname", [](const ParsedURL& u) { return u.hostname; },
     [](ParsedURL* u, const std::string& v) { u->hostname = v; }},
    {"port", [](const ParsedURL& u) { return u.port; },
     [](ParsedURL* u, const std::string& v) { u->port = v; }},
    {"pathname", [](const ParsedURL& u) { return u.pathname; },
     [](ParsedURL* u, const std::string& v) {
       u->pathname = !v.empty() && v[0] == '/' ? v : "/" + v;
     }},
    {"search", [](const ParsedURL& u) { return u.Search(); },
     [](ParsedURL* u, const std::string& v) {
       u->query = StripLeading(v, '?');
     }},
    {"hash", [](const ParsedURL& u) { return u.Hash(); },
     [](ParsedURL* u, const std::string& v) {
       u->fragment = StripLeading(v, '#');
     }},
};

CALLBACK(URLPartGetter) {
  const URLPart& part = kURLParts[DataInt(info)];
  info.GetReturnValue().Set(ToV8StringFromUtf8(
      info.GetIsolate(), part.get(Receiver<URLObject>(info)->url)));
}

CALLBACK(URLPartSetter) {
  const URLPart& part = kURLParts[DataInt(info)];
  if (!part.set) return;
  std::string value;
  if (!ToStdString(info, info[0], &value)) return;
  URLObject* url = Receiver<URLObject>(info);
  part.set(&url->url, value);
  url->QueryChanged();
}

CALLBACK(URLHref) {
  info.GetReturnValue().Set(ToV8StringFromUtf8(
      info.GetIsolate(), Receiver<URLObject>(info)->url.Href()));
}

// Reads the URLSearchParams constructor argument: a string, another
// URLSearchParams, a sequence of pairs or a record.
bool InitSearchParams(const CallbackInfo& info, URLSearchParams* params,
                      Local<Value> init) {
  Isolate* isolate = info.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  if (init->IsUndefined() || init->IsNull()) return true;
  if (URLSearchParams* other =
          To<URLSearchParams>(isolate, init, Interface::kURLSearchParams)) {
    params->pairs = other->pairs;
    return true;
  }
  if (init->IsArray()) {
    Local<Array> array = init.As<Array>();
    for (uint32_t i = 0; i < array->Length(); ++i) {
      Local<Value> pair;
      if (!array->Get(context, i).ToLocal(&pair)) return false;
      if (!pair->IsArray() || pair.As<Array>()->Length() != 2) {
        ThrowTypeError(isolate,
                       "Failed to construct 'URLSearchParams': The provided "
                       "value cannot be converted to a sequence.");
        return false;
      }
      Local<Value> name, value;
      std::string name_string, value_string;
      if (!pair.As<Array>()->Get(context, 0).ToLocal(&name) ||
          !pair.As<Array>()->Get(context, 1).ToLocal(&value) ||
          !ToStdString(info, name, &name_string) ||
          !ToStdString(info, value, &value_string)) {
        return false;
      }
      params->pairs.emplace_back(name_string, value_string);
    }
    return true;
  }
  if (init->IsObject()) {
    Local<Object> record = init.As<Object>();
    Local<Array> keys;
    if (!record->GetOwnPropertyNames(context).ToLocal(&keys)) return false;
    for (uint32_t i = 0; i < keys->Length(); ++i) {
      Local<Value> key, value;
      std::string key_string, value_string;
      if (!keys->Get(context, i).ToLocal(&key) ||
          !record->Get(context, key).ToLocal(&value) ||
          !ToStdString(info, key, &key_string) ||
          !ToStdString(info, value, &value_string)) {
        return false;
      }
      params->pairs.emplace_back(key_string, value_string);
    }
    return true;
  }
  std::string query;
  if (!ToStdString(info, init, &query)) return false;
  params->pairs = ParseQuery(query);
  return true;
}

CALLBACK(URLSearchParamsConstructor) {
  if (!RequireConstructCall(info)) return;
  Isolate* isolate = info.GetIsolate();
  URLSearchParams* params = MakeGarbageCollected<URLSearchParams>(isolate);
  if (info.Length() > 0 && !InitSearchParams(info, params, info[0])) return;
  WrapThis(info, params);
}

// Builds the array that keys()/values()/entries() iterate over and returns
// its array iterator.
void ReturnIterator(const CallbackInfo& info, int kind) {
  Isolate* isolate = info.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  URLSearchParams* params = Receiver<URLSearchParams>(info);
  LocalVector<Value> items(isolate);
  for (const auto& pair : params->pairs) {
    Local<Value> name = ToV8StringFromUtf8(isolate, pair.first);
    Local<Value> value = ToV8StringFromUtf8(isolate, pair.second);
    if (kind == 0) {
      items.push_back(name);
    } else if (kind == 1) {
      items.push_back(value);
    } else {
      Local<Value> entry[] = {name, value};
      items.push_back(Array::New(isolate, entry, 2));
    }
  }
  Local<Array> array = Array::New(isolate, items.data(), items.size());
  Local<Value> iterator_function;
  Local<Value> iterator;
  if (array->Get(context, Symbol::GetIterator(isolate))
          .ToLocal(&iterator_function) &&
      iterator_function.As<Function>()
          ->Call(context, array, 0, nullptr)
          .ToLocal(&iterator)) {
    info.GetReturnValue().Set(iterator);
  }
}

void InstallURLBindings(Isolate* isolate) {
  const Interface kURL = Interface::kURL;
  SetConstructor(isolate, kURL, URLConstructor, 1);
  for (size_t i = 0; i < arraysize(kURLParts); ++i) {
    DefineAccessor(isolate, kURL, kURLParts[i].name, URLPartGetter,
                   URLPartSetter, Integer::New(isolate, static_cast<int>(i)));
  }
  DefineMethod(isolate, kURL, "toString", URLHref);
  DefineMethod(isolate, kURL, "toJSON", URLHref);
  DefineAccessor(isolate, kURL, "searchParams", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    URLObject* url = Receiver<URLObject>(info);
    if (!url->search_params) {
      URLSearchParams* params = MakeGarbageCollected<URLSearchParams>(isolate);
      params->pairs = ParseQuery(url->url.query);
      params->url = url;
      url->search_params = params;
    }
    Return(info, url->search_params.Get());
  });
  DefineStaticMethod(
      isolate, kURL, "canParse",
      [](const CallbackInfo& info) {
        std::string input, base;
        if (info.Length() < 1 || !ToStdString(info, info[0], &input)) return;
        ParsedURL parsed;
        if (info.Length() > 1 && !info[1]->IsUndefined()) {
          if (!ToStdString(info, info[1], &base)) return;
          ParsedURL parsed_base = ParsedURL::Parse(base);
          if (parsed_base.valid) {
            parsed = ParsedURL::Resolve(input, parsed_base);
          }
        } else {
          parsed = ParsedURL::Parse(input);
        }
        info.GetReturnValue().Set(parsed.valid);
      },
      1);

  const Interface kParams = Interface::kURLSearchParams;
  SetConstructor(isolate, kParams, URLSearchParamsConstructor);
  DefineAccessor(isolate, kParams, "size", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        static_cast<uint32_t>(Receiver<URLSearchParams>(info)->pairs.size()));
  });
  DefineMethod(
      isolate, kParams, "append",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 2)) return;
        std::string name, value;
        if (!ToStdString(info, info[0], &name) ||
            !ToStdString(info, info[1], &value)) {
          return;
        }
        URLSearchParams* params = Receiver<URLSearchParams>(info);
        params->pairs.emplace_back(name, value);
        params->Update();
      },
      2);
  DefineMethod(
      isolate, kParams, "delete",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        std::string name;
        if (!ToStdString(info, info[0], &name)) return;
        URLSearchParams* params = Receiver<URLSearchParams>(info);
        params->pairs.erase(
            std::remove_if(
                params->pairs.begin(), params->pairs.end(),
                [&](const auto& pair) { return pair.first == name; }),
            params->pairs.end());
        params->Update();
      },
      1);
  DefineMethod(
      isolate, kParams, "get",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        std::string name;
        if (!ToStdString(info, info[0], &name)) return;
        for (const auto& pair : Receiver<URLSearchParams>(info)->pairs) {
          if (pair.first == name) {
            info.GetReturnValue().Set(
                ToV8StringFromUtf8(info.GetIsolate(), pair.second));
            return;
          }
        }
        info.GetReturnValue().SetNull();
      },
      1);
  DefineMethod(
      isolate, kParams, "getAll",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 1)) return;
        std::string name;
        if (!ToStdString(info, info[0], &name)) return;
        LocalVector<Value> values(isolate);
        for (const auto& pair : Receiver<URLSearchParams>(info)->pairs) {
          if (pair.first == name) {
            values.push_back(ToV8StringFromUtf8(isolate, pair.second));
          }
        }
        info.GetReturnValue().Set(
            Array::New(isolate, values.data(), values.size()));
      },
      1);
  DefineMethod(
      isolate, kParams, "has",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        std::string name;
        if (!ToStdString(info, info[0], &name)) return;
        for (const auto& pair : Receiver<URLSearchParams>(info)->pairs) {
          if (pair.first == name) {
            info.GetReturnValue().Set(true);
            return;
          }
        }
        info.GetReturnValue().Set(false);
      },
      1);
  DefineMethod(
      isolate, kParams, "set",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 2)) return;
        std::string name, value;
        if (!ToStdString(info, info[0], &name) ||
            !ToStdString(info, info[1], &value)) {
          return;
        }
        URLSearchParams* params = Receiver<URLSearchParams>(info);
        bool found = false;
        auto& pairs = params->pairs;
        for (size_t i = 0; i < pairs.size();) {
          if (pairs[i].first != name) {
            ++i;
          } else if (!found) {
            pairs[i].second = value;
            found = true;
            ++i;
          } else {
            pairs.erase(pairs.begin() + i);
          }
        }
        if (!found) pairs.emplace_back(name, value);
        params->Update();
      },
      2);
  DefineMethod(isolate, kParams, "sort", [](const CallbackInfo& info) {
    URLSearchParams* params = Receiver<URLSearchParams>(info);
    std::stable_sort(
        params->pairs.begin(), params->pairs.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    params->Update();
  });
  DefineMethod(isolate, kParams, "toString", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(ToV8StringFromUtf8(
        info.GetIsolate(),
        SerializeQuery(Receiver<URLSearchParams>(info)->pairs)));
  });
  DefineMethod(
      isolate, kParams, "forEach",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (info.Length() < 1 || !info[0]->IsFunction()) {
          ThrowTypeError(isolate, "parameter 1 is not of type 'Function'.");
          return;
        }
        Local<Context> context = isolate->GetCurrentContext();
        URLSearchParams* params = Receiver<URLSearchParams>(info);
        Local<Value> this_arg =
            info.Length() > 1 ? info[1] : Undefined(isolate).As<Value>();
        auto pairs = params->pairs;
        for (const auto& pair : pairs) {
          Local<Value> args[] = {ToV8StringFromUtf8(isolate, pair.second),
                                 ToV8StringFromUtf8(isolate, pair.first),
                                 info.This()};
          if (info[0]
                  .As<Function>()
                  ->Call(context, this_arg, 3, args)
                  .IsEmpty()) {
            return;
          }
        }
      },
      1);
  DefineMethod(isolate, kParams, "keys",
               [](const CallbackInfo& info) { ReturnIterator(info, 0); });
  DefineMethod(isolate, kParams, "values",
               [](const CallbackInfo& info) { ReturnIterator(info, 1); });
  DefineMethod(isolate, kParams, "entries",
               [](const CallbackInfo& info) { ReturnIterator(info, 2); });
  InterfaceTemplate(isolate, kParams)
      ->PrototypeTemplate()
      ->Set(Symbol::GetIterator(isolate),
            FunctionTemplate::New(
                isolate,
                [](const CallbackInfo& info) { ReturnIterator(info, 2); },
                Local<Value>(),
                Signature::New(isolate, InterfaceTemplate(isolate, kParams))),
            DontEnum);
}

// --- DOMParser and XMLSerializer ---------------------------------------------

// Marker object for the two stateless parser/serializer interfaces.
class Stateless final : public ScriptWrappable {
 public:
  explicit Stateless(Interface interface) : ScriptWrappable(interface) {}
};

template <Interface interface>
CALLBACK(StatelessConstructor) {
  if (!RequireConstructCall(info)) return;
  WrapThis(info, MakeGarbageCollected<Stateless>(info.GetIsolate(), interface));
}

void InstallParserBindings(Isolate* isolate) {
  SetConstructor(isolate, Interface::kDOMParser,
                 StatelessConstructor<Interface::kDOMParser>);
  DefineMethod(
      isolate, Interface::kDOMParser, "parseFromString",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 2)) return;
        std::string markup, type;
        if (!ToStdString(info, info[0], &markup) ||
            !ToStdString(info, info[1], &type)) {
          return;
        }
        if (type != "text/html") {
          // XML documents would need an XML parser and namespace-aware
          // tree building.
          ThrowDOMException(isolate, "NotSupportedError",
                            "mock-dom: DOMParser only supports text/html, "
                            "got '" +
                                type + "'.");
          return;
        }
        Document* document = MakeGarbageCollected<Document>(
            isolate, isolate, Window::Current(isolate));
        ParseHTMLDocument(isolate, document, markup);
        document->ready_state = AtomicString("complete");
        Return(info, document);
      },
      2);

  SetConstructor(isolate, Interface::kXMLSerializer,
                 StatelessConstructor<Interface::kXMLSerializer>);
  DefineMethod(
      isolate, Interface::kXMLSerializer, "serializeToString",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
        if (!node) return;
        // HTML serialization; elements are not rewritten into XML syntax.
        std::string out;
        SerializeNode(node, &out);
        Return(info, String::FromUtf8(out));
      },
      1);
}

// --- CSSStyleSheet -----------------------------------------------------------

void InstallStyleSheetBindings(Isolate* isolate) {
  const Interface kSheet = Interface::kCSSStyleSheet;
  SetConstructor(isolate, kSheet, [](const CallbackInfo& info) {
    if (!RequireConstructCall(info)) return;
    WrapThis(info,
             MakeGarbageCollected<CSSStyleSheet>(info.GetIsolate(), nullptr));
  });
  DefineMethod(
      isolate, kSheet, "replaceSync",
      [](const CallbackInfo& info) {
        std::string text;
        if (!RequireArguments(info, 1) || !ToStdString(info, info[0], &text)) {
          return;
        }
        Receiver<CSSStyleSheet>(info)->SetText(text);
      },
      1);
  DefineMethod(
      isolate, kSheet, "replace",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        std::string text;
        if (!RequireArguments(info, 1) || !ToStdString(info, info[0], &text)) {
          return;
        }
        Receiver<CSSStyleSheet>(info)->SetText(text);
        Local<Context> context = isolate->GetCurrentContext();
        Local<Promise::Resolver> resolver =
            Promise::Resolver::New(context).ToLocalChecked();
        USE(resolver->Resolve(context, info.This()));
        info.GetReturnValue().Set(resolver->GetPromise());
      },
      1);
  DefineMethod(
      isolate, kSheet, "insertRule",
      [](const CallbackInfo& info) {
        std::string rule;
        if (!RequireArguments(info, 1) || !ToStdString(info, info[0], &rule)) {
          return;
        }
        CSSStyleSheet* sheet = Receiver<CSSStyleSheet>(info);
        int index = 0;
        if (info.Length() > 1 && !ToInt(info, info[1], &index)) return;
        if (index < 0 || !sheet->InsertRule(rule, static_cast<size_t>(index))) {
          ThrowDOMException(info.GetIsolate(), "IndexSizeError",
                            "The index provided is larger than the maximum "
                            "index.");
          return;
        }
        info.GetReturnValue().Set(index);
      },
      1);
  DefineMethod(
      isolate, kSheet, "deleteRule",
      [](const CallbackInfo& info) {
        CSSStyleSheet* sheet = Receiver<CSSStyleSheet>(info);
        int index = 0;
        if (info.Length() > 0 && !ToInt(info, info[0], &index)) return;
        if (index < 0 || !sheet->DeleteRule(static_cast<size_t>(index))) {
          ThrowDOMException(info.GetIsolate(), "IndexSizeError",
                            "The index provided is larger than the maximum "
                            "index.");
          return;
        }
      },
      1);
  DefineAccessor(isolate, kSheet, "cssRules", [](const CallbackInfo& info) {
    // Rules are exposed as plain {cssText} objects.
    Isolate* isolate = info.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    LocalVector<Value> rules(isolate);
    for (const std::string& text :
         Receiver<CSSStyleSheet>(info)->rule_texts()) {
      Local<Object> rule = Object::New(isolate);
      USE(rule->Set(context, V8Name(isolate, "cssText"),
                    ToV8StringFromUtf8(isolate, text)));
      rules.push_back(rule);
    }
    info.GetReturnValue().Set(Array::New(isolate, rules.data(), rules.size()));
  });
}

#undef CALLBACK

}  // namespace

void InstallMiscBindings(Isolate* isolate) {
  InstallURLBindings(isolate);
  InstallParserBindings(isolate);
  InstallStyleSheetBindings(isolate);
}

}  // namespace mock_dom
}  // namespace v8
