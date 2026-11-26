// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bindings for Element, HTMLElement and friends, and the objects that hang
// off elements (classList, style, dataset, attributes).

#include <cmath>

#include "experimental/d8/mock-dom/bindings.h"
#include "experimental/d8/mock-dom/css.h"
#include "experimental/d8/mock-dom/html.h"
#include "experimental/d8/mock-dom/isolate-data.h"
#include "experimental/d8/mock-dom/layout.h"
#include "experimental/d8/mock-dom/objects.h"
#include "include/v8-container.h"
#include "include/v8-exception.h"
#include "include/v8-primitive.h"

namespace v8 {
namespace mock_dom {

namespace {

#define CALLBACK(name) void name(const CallbackInfo& info)

template <typename T>
T* HolderImpl(const PropertyCallbackInfo<Value>& info) {
  return static_cast<T*>(static_cast<ScriptWrappable*>(
      Object::Unwrap<kWrapperTag, Object::Wrappable>(info.GetIsolate(),
                                                     info.Holder())));
}

// --- Attributes --------------------------------------------------------------

CALLBACK(ElementGetAttribute) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Element* element = Receiver<Element>(info);
  Local<v8::String> name;
  if (!info[0]->ToString(isolate->GetCurrentContext()).ToLocal(&name)) return;
  ReturnOrNull(info, element->GetAttribute(
                         element->NormalizeAttributeName(isolate, name)));
}

CALLBACK(ElementSetAttribute) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 2)) return;
  Element* element = Receiver<Element>(info);
  Local<v8::String> name;
  AtomicString value;
  if (!info[0]->ToString(isolate->GetCurrentContext()).ToLocal(&name) ||
      !ToAtom(info, info[1], &value)) {
    return;
  }
  element->SetAttribute(isolate, element->NormalizeAttributeName(isolate, name),
                        value);
}

CALLBACK(ElementRemoveAttribute) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Element* element = Receiver<Element>(info);
  Local<v8::String> name;
  if (!info[0]->ToString(isolate->GetCurrentContext()).ToLocal(&name)) return;
  element->RemoveAttribute(isolate,
                           element->NormalizeAttributeName(isolate, name));
}

CALLBACK(ElementHasAttribute) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Element* element = Receiver<Element>(info);
  Local<v8::String> name;
  if (!info[0]->ToString(isolate->GetCurrentContext()).ToLocal(&name)) return;
  info.GetReturnValue().Set(
      element->HasAttribute(element->NormalizeAttributeName(isolate, name)));
}

CALLBACK(ElementToggleAttribute) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Element* element = Receiver<Element>(info);
  Local<v8::String> name_string;
  if (!info[0]->ToString(isolate->GetCurrentContext()).ToLocal(&name_string)) {
    return;
  }
  AtomicString name = element->NormalizeAttributeName(isolate, name_string);
  bool has = element->HasAttribute(name);
  bool want = info.Length() > 1 && !info[1]->IsUndefined()
                  ? info[1]->BooleanValue(isolate)
                  : !has;
  if (want && !has) {
    element->SetAttribute(isolate, name, AtomicString::EmptyAtom());
  } else if (!want && has) {
    element->RemoveAttribute(isolate, name);
  }
  info.GetReturnValue().Set(want);
}

// The *NS variants ignore the namespace except for stripping a prefix from
// the qualified name: attributes are stored by qualified name ("xlink:href").
CALLBACK(ElementGetAttributeNS) {
  if (!RequireArguments(info, 2)) return;
  AtomicString name;
  if (!ToAtom(info, info[1], &name)) return;
  Element* element = Receiver<Element>(info);
  const AtomicString& value = element->GetAttribute(name);
  if (!value.is_null() || !name.is_8bit()) {
    ReturnOrNull(info, value);
    return;
  }
  // Look for a prefixed attribute with this local name.
  element->SynchronizeAttributes();
  for (const Attribute& attribute : element->attributes()) {
    if (!attribute.name.is_8bit()) continue;
    std::string_view view = attribute.name.view8();
    size_t colon = view.find(':');
    if (colon != std::string_view::npos &&
        view.substr(colon + 1) == name.view8()) {
      Return(info, attribute.value);
      return;
    }
  }
  info.GetReturnValue().SetNull();
}

CALLBACK(ElementSetAttributeNS) {
  if (!RequireArguments(info, 3)) return;
  AtomicString name, value;
  if (!ToAtom(info, info[1], &name) || !ToAtom(info, info[2], &value)) return;
  Receiver<Element>(info)->SetAttribute(info.GetIsolate(), name, value);
}

CALLBACK(ElementRemoveAttributeNS) {
  if (!RequireArguments(info, 2)) return;
  AtomicString name;
  if (!ToAtom(info, info[1], &name)) return;
  Receiver<Element>(info)->RemoveAttribute(info.GetIsolate(), name);
}

CALLBACK(ElementHasAttributeNS) {
  if (!RequireArguments(info, 2)) return;
  AtomicString name;
  if (!ToAtom(info, info[1], &name)) return;
  info.GetReturnValue().Set(Receiver<Element>(info)->HasAttribute(name));
}

CALLBACK(ElementGetAttributeNames) {
  Isolate* isolate = info.GetIsolate();
  Element* element = Receiver<Element>(info);
  element->SynchronizeAttributes();
  LocalVector<Value> names(isolate);
  for (const Attribute& attribute : element->attributes()) {
    names.push_back(ToV8String(isolate, attribute.name));
  }
  info.GetReturnValue().Set(Array::New(isolate, names.data(), names.size()));
}

CALLBACK(ElementAttributes) {
  Isolate* isolate = info.GetIsolate();
  Element* element = Receiver<Element>(info);
  ElementRareData* rare = element->EnsureRareData(isolate);
  if (!rare->attributes) {
    rare->attributes = MakeGarbageCollected<NamedNodeMap>(isolate, element);
  }
  Return(info, rare->attributes.Get());
}

CALLBACK(ElementGetAttributeNode) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Element* element = Receiver<Element>(info);
  Local<v8::String> name_string;
  if (!info[0]->ToString(isolate->GetCurrentContext()).ToLocal(&name_string)) {
    return;
  }
  AtomicString name = element->NormalizeAttributeName(isolate, name_string);
  if (!element->HasAttribute(name)) {
    info.GetReturnValue().SetNull();
    return;
  }
  Return(info, MakeGarbageCollected<Attr>(isolate, element, name));
}

Intercepted NamedNodeMapIndexedGetter(uint32_t index,
                                      const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  Element* element = HolderImpl<NamedNodeMap>(info)->element();
  element->SynchronizeAttributes();
  if (index >= element->attributes().size()) return Intercepted::kNo;
  info.GetReturnValue().Set(
      ToV8(isolate, MakeGarbageCollected<Attr>(
                        isolate, element, element->attributes()[index].name)));
  return Intercepted::kYes;
}

void InstallAttributeBindings(Isolate* isolate) {
  const Interface kElement = Interface::kElement;
  DefineMethod(isolate, kElement, "getAttribute", ElementGetAttribute, 1);
  DefineMethod(isolate, kElement, "setAttribute", ElementSetAttribute, 2);
  DefineMethod(isolate, kElement, "removeAttribute", ElementRemoveAttribute, 1);
  DefineMethod(isolate, kElement, "hasAttribute", ElementHasAttribute, 1);
  DefineMethod(isolate, kElement, "toggleAttribute", ElementToggleAttribute, 1);
  DefineMethod(isolate, kElement, "getAttributeNS", ElementGetAttributeNS, 2);
  DefineMethod(isolate, kElement, "setAttributeNS", ElementSetAttributeNS, 3);
  DefineMethod(isolate, kElement, "removeAttributeNS", ElementRemoveAttributeNS,
               2);
  DefineMethod(isolate, kElement, "hasAttributeNS", ElementHasAttributeNS, 2);
  DefineMethod(isolate, kElement, "getAttributeNames",
               ElementGetAttributeNames);
  DefineMethod(isolate, kElement, "hasAttributes",
               [](const CallbackInfo& info) {
                 Element* element = Receiver<Element>(info);
                 element->SynchronizeAttributes();
                 info.GetReturnValue().Set(!element->attributes().empty());
               });
  DefineMethod(isolate, kElement, "getAttributeNode", ElementGetAttributeNode,
               1);
  DefineAccessor(isolate, kElement, "attributes", ElementAttributes);
  DefineMethod(
      isolate, kElement, "removeAttributeNode",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        Attr* attr = info.Length()
                         ? To<Attr>(isolate, info[0], Interface::kAttr)
                         : nullptr;
        Element* element = Receiver<Element>(info);
        if (!attr || attr->element() != element ||
            !element->HasAttribute(attr->name())) {
          ThrowDOMException(isolate, "NotFoundError",
                            "The node provided is not an attribute of this "
                            "element.");
          return;
        }
        element->RemoveAttribute(isolate, attr->name());
        info.GetReturnValue().Set(info[0]);
      },
      1);

  const Interface kMap = Interface::kNamedNodeMap;
  InterfaceTemplate(isolate, kMap)
      ->InstanceTemplate()
      ->SetHandler(
          IndexedPropertyHandlerConfiguration(NamedNodeMapIndexedGetter));
  DefineAccessor(isolate, kMap, "length", [](const CallbackInfo& info) {
    Element* element = Receiver<NamedNodeMap>(info)->element();
    element->SynchronizeAttributes();
    info.GetReturnValue().Set(
        static_cast<uint32_t>(element->attributes().size()));
  });
  DefineMethod(
      isolate, kMap, "item",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        Element* element = Receiver<NamedNodeMap>(info)->element();
        element->SynchronizeAttributes();
        double index = 0;
        if (info.Length() > 0 && !ToNumber(info, info[0], &index)) return;
        if (index < 0 || index >= element->attributes().size()) {
          info.GetReturnValue().SetNull();
          return;
        }
        Return(info,
               MakeGarbageCollected<Attr>(
                   isolate, element,
                   element->attributes()[static_cast<size_t>(index)].name));
      },
      1);
  DefineMethod(
      isolate, kMap, "getNamedItem",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 1)) return;
        Element* element = Receiver<NamedNodeMap>(info)->element();
        AtomicString name;
        if (!ToAtom(info, info[0], &name)) return;
        if (!element->HasAttribute(name)) {
          info.GetReturnValue().SetNull();
          return;
        }
        Return(info, MakeGarbageCollected<Attr>(isolate, element, name));
      },
      1);

  const Interface kAttr = Interface::kAttr;
  auto attr_name = [](const CallbackInfo& info) {
    Return(info, Receiver<Attr>(info)->name());
  };
  DefineAccessor(isolate, kAttr, "name", attr_name);
  DefineAccessor(isolate, kAttr, "localName", attr_name);
  DefineAccessor(isolate, kAttr, "nodeName", attr_name);
  auto attr_value_getter = [](const CallbackInfo& info) {
    Attr* attr = Receiver<Attr>(info);
    const AtomicString& value = attr->element()->GetAttribute(attr->name());
    Return(info, value.is_null() ? AtomicString::EmptyAtom() : value);
  };
  auto attr_value_setter = [](const CallbackInfo& info) {
    Attr* attr = Receiver<Attr>(info);
    AtomicString value;
    if (!ToAtom(info, info[0], &value)) return;
    attr->element()->SetAttribute(info.GetIsolate(), attr->name(), value);
  };
  DefineAccessor(isolate, kAttr, "value", attr_value_getter, attr_value_setter);
  DefineAccessor(isolate, kAttr, "nodeValue", attr_value_getter,
                 attr_value_setter);
  DefineAccessor(isolate, kAttr, "ownerElement", [](const CallbackInfo& info) {
    Return(info, Receiver<Attr>(info)->element());
  });
  DefineAccessor(isolate, kAttr, "specified", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(true);
  });
  DefineAccessor(isolate, kAttr, "namespaceURI", [](const CallbackInfo& info) {
    info.GetReturnValue().SetNull();
  });
}

// --- classList ---------------------------------------------------------------

bool TokensFromArguments(const CallbackInfo& info,
                         std::vector<AtomicString>* tokens) {
  for (int i = 0; i < info.Length(); ++i) {
    AtomicString token;
    if (!ToAtom(info, info[i], &token)) return false;
    if (token.empty()) {
      ThrowDOMException(info.GetIsolate(), "SyntaxError",
                        "The token provided must not be empty.");
      return false;
    }
    if (token.is_8bit()) {
      for (char c : token.view8()) {
        if (IsHtmlSpace(c)) {
          ThrowDOMException(info.GetIsolate(), "InvalidCharacterError",
                            "The token provided ('" + token.ToUtf8() +
                                "') contains HTML space characters, which are "
                                "not valid in tokens.");
          return false;
        }
      }
    }
    tokens->push_back(token);
  }
  return true;
}

Intercepted TokenListIndexedGetter(uint32_t index,
                                   const PropertyCallbackInfo<Value>& info) {
  DOMTokenList* list = HolderImpl<DOMTokenList>(info);
  if (index >= list->tokens().size()) return Intercepted::kNo;
  info.GetReturnValue().Set(
      ToV8String(info.GetIsolate(), list->tokens()[index]));
  return Intercepted::kYes;
}

void InstallTokenListBindings(Isolate* isolate) {
  const Interface kList = Interface::kDOMTokenList;
  InterfaceTemplate(isolate, kList)
      ->InstanceTemplate()
      ->SetHandler(IndexedPropertyHandlerConfiguration(TokenListIndexedGetter));
  DefineAccessor(isolate, Interface::kElement, "classList",
                 [](const CallbackInfo& info) {
                   Isolate* isolate = info.GetIsolate();
                   Element* element = Receiver<Element>(info);
                   ElementRareData* rare = element->EnsureRareData(isolate);
                   if (!rare->class_list) {
                     rare->class_list =
                         MakeGarbageCollected<DOMTokenList>(isolate, element);
                   }
                   Return(info, rare->class_list.Get());
                 });
  DefineAccessor(isolate, kList, "length", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        static_cast<uint32_t>(Receiver<DOMTokenList>(info)->tokens().size()));
  });
  DefineAccessor(
      isolate, kList, "value",
      [](const CallbackInfo& info) {
        Return(info, Receiver<DOMTokenList>(info)->Value());
      },
      [](const CallbackInfo& info) {
        AtomicString value;
        if (!ToAtom(info, info[0], &value)) return;
        Receiver<DOMTokenList>(info)->SetValue(info.GetIsolate(), value);
      });
  DefineMethod(isolate, kList, "toString", [](const CallbackInfo& info) {
    Return(info, Receiver<DOMTokenList>(info)->Value());
  });
  DefineMethod(
      isolate, kList, "item",
      [](const CallbackInfo& info) {
        DOMTokenList* list = Receiver<DOMTokenList>(info);
        double index = 0;
        if (info.Length() > 0 && !ToNumber(info, info[0], &index)) return;
        if (index < 0 || index >= list->tokens().size()) {
          info.GetReturnValue().SetNull();
          return;
        }
        Return(info, list->tokens()[static_cast<size_t>(index)]);
      },
      1);
  DefineMethod(
      isolate, kList, "contains",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        AtomicString token;
        if (!ToAtom(info, info[0], &token)) return;
        info.GetReturnValue().Set(
            Receiver<DOMTokenList>(info)->Contains(token));
      },
      1);
  DefineMethod(isolate, kList, "add", [](const CallbackInfo& info) {
    std::vector<AtomicString> tokens;
    if (!TokensFromArguments(info, &tokens)) return;
    Receiver<DOMTokenList>(info)->Add(info.GetIsolate(), tokens);
  });
  DefineMethod(isolate, kList, "remove", [](const CallbackInfo& info) {
    std::vector<AtomicString> tokens;
    if (!TokensFromArguments(info, &tokens)) return;
    Receiver<DOMTokenList>(info)->Remove(info.GetIsolate(), tokens);
  });
  DefineMethod(
      isolate, kList, "toggle",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 1)) return;
        std::vector<AtomicString> tokens;
        AtomicString token;
        if (!ToAtom(info, info[0], &token)) return;
        bool has_force = info.Length() > 1 && !info[1]->IsUndefined();
        bool force = has_force && info[1]->BooleanValue(isolate);
        info.GetReturnValue().Set(Receiver<DOMTokenList>(info)->Toggle(
            isolate, token, has_force, force));
      },
      1);
  DefineMethod(
      isolate, kList, "replace",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 2)) return;
        AtomicString token, new_token;
        if (!ToAtom(info, info[0], &token) ||
            !ToAtom(info, info[1], &new_token)) {
          return;
        }
        info.GetReturnValue().Set(Receiver<DOMTokenList>(info)->Replace(
            info.GetIsolate(), token, new_token));
      },
      2);
}

// --- style -------------------------------------------------------------------

// Whether |name| could be a CSS property in its JS spelling. Without a table
// of every CSS property, any identifier made of letters and dashes is
// treated as one; `'anything' in el.style` is therefore true here.
bool LooksLikeCSSProperty(std::string_view name) {
  if (name.empty()) return false;
  for (char c : name) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-')) {
      return false;
    }
  }
  return true;
}

Intercepted StyleNamedGetter(Local<Name> property,
                             const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  std::string name = ToUtf8(isolate, property.As<v8::String>());
  if (!LooksLikeCSSProperty(name)) return Intercepted::kNo;
  CSSStyleDeclaration* style = HolderImpl<CSSStyleDeclaration>(info);
  info.GetReturnValue().Set(ToV8String(
      isolate,
      style->GetPropertyValue(CSSStyleDeclaration::PropertyNameFromJS(name))));
  return Intercepted::kYes;
}

Intercepted StyleNamedSetter(Local<Name> property, Local<Value> value,
                             const PropertyCallbackInfo<Boolean>& info) {
  Isolate* isolate = info.GetIsolate();
  std::string name = ToUtf8(isolate, property.As<v8::String>());
  if (!LooksLikeCSSProperty(name)) return Intercepted::kNo;
  CSSStyleDeclaration* style =
      static_cast<CSSStyleDeclaration*>(static_cast<ScriptWrappable*>(
          Object::Unwrap<kWrapperTag, Object::Wrappable>(isolate,
                                                         info.Holder())));
  String text = String::Empty();
  if (!value->IsNull()) {
    Local<v8::String> string;
    if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&string)) {
      return Intercepted::kYes;
    }
    text = ToDOMString(isolate, string);
  }
  style->SetProperty(isolate, CSSStyleDeclaration::PropertyNameFromJS(name),
                     text, false);
  return Intercepted::kYes;
}

bool CSSNameArgument(const CallbackInfo& info, AtomicString* name) {
  std::string text;
  if (!ToStdString(info, info[0], &text)) return false;
  *name = text.substr(0, 2) == "--"
              ? AtomicString::FromUtf8(text)
              : AtomicString(std::string_view(AsciiLower(text)));
  return true;
}

void InstallStyleBindings(Isolate* isolate) {
  const Interface kStyle = Interface::kCSSStyleDeclaration;
  InterfaceTemplate(isolate, kStyle)
      ->InstanceTemplate()
      ->SetHandler(NamedPropertyHandlerConfiguration(
          StyleNamedGetter, StyleNamedSetter, nullptr, nullptr, nullptr,
          Local<Value>(),
          static_cast<PropertyHandlerFlags>(
              static_cast<int>(PropertyHandlerFlags::kNonMasking) |
              static_cast<int>(PropertyHandlerFlags::kOnlyInterceptStrings))));
  DefineAccessor(
      isolate, kStyle, "cssText",
      [](const CallbackInfo& info) {
        Return(info, Receiver<CSSStyleDeclaration>(info)->CssText());
      },
      [](const CallbackInfo& info) {
        String text;
        if (!ToStringNullAsEmpty(info, info[0], &text)) return;
        Receiver<CSSStyleDeclaration>(info)->SetCssText(info.GetIsolate(),
                                                        text);
      });
  DefineAccessor(isolate, kStyle, "length", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(static_cast<uint32_t>(
        Receiver<CSSStyleDeclaration>(info)->properties().size()));
  });
  DefineMethod(
      isolate, kStyle, "item",
      [](const CallbackInfo& info) {
        CSSStyleDeclaration* style = Receiver<CSSStyleDeclaration>(info);
        double index = 0;
        if (info.Length() > 0 && !ToNumber(info, info[0], &index)) return;
        if (index < 0 || index >= style->properties().size()) {
          Return(info, String::Empty());
          return;
        }
        Return(info, style->properties()[static_cast<size_t>(index)].name);
      },
      1);
  DefineMethod(
      isolate, kStyle, "getPropertyValue",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        AtomicString name;
        if (!CSSNameArgument(info, &name)) return;
        Return(info,
               Receiver<CSSStyleDeclaration>(info)->GetPropertyValue(name));
      },
      1);
  DefineMethod(
      isolate, kStyle, "getPropertyPriority",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        AtomicString name;
        if (!CSSNameArgument(info, &name)) return;
        Return(
            info,
            AtomicString(
                Receiver<CSSStyleDeclaration>(info)->GetPropertyPriority(name)
                    ? "important"
                    : ""));
      },
      1);
  DefineMethod(
      isolate, kStyle, "setProperty",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 2)) return;
        AtomicString name;
        String value;
        std::string priority;
        if (!CSSNameArgument(info, &name) ||
            !ToStringNullAsEmpty(info, info[1], &value)) {
          return;
        }
        if (info.Length() > 2 && !info[2]->IsNullOrUndefined() &&
            !ToStdString(info, info[2], &priority)) {
          return;
        }
        Receiver<CSSStyleDeclaration>(info)->SetProperty(
            info.GetIsolate(), name, value,
            EqualsIgnoringAsciiCase(priority, "important"));
      },
      2);
  DefineMethod(
      isolate, kStyle, "removeProperty",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        AtomicString name;
        if (!CSSNameArgument(info, &name)) return;
        Return(info, Receiver<CSSStyleDeclaration>(info)->RemoveProperty(
                         info.GetIsolate(), name));
      },
      1);
}

// --- dataset -----------------------------------------------------------------

Intercepted DatasetGetter(Local<Name> property,
                          const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  Element* element = HolderImpl<DOMStringMap>(info)->element();
  const AtomicString& value = element->GetAttribute(
      DOMStringMap::AttributeName(ToUtf8(isolate, property.As<v8::String>())));
  if (value.is_null()) return Intercepted::kNo;
  info.GetReturnValue().Set(ToV8String(isolate, value));
  return Intercepted::kYes;
}

Intercepted DatasetSetter(Local<Name> property, Local<Value> value,
                          const PropertyCallbackInfo<Boolean>& info) {
  Isolate* isolate = info.GetIsolate();
  Element* element = static_cast<DOMStringMap*>(
                         static_cast<ScriptWrappable*>(
                             Object::Unwrap<kWrapperTag, Object::Wrappable>(
                                 isolate, info.Holder())))
                         ->element();
  Local<v8::String> string;
  if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&string)) {
    return Intercepted::kYes;
  }
  element->SetAttribute(
      isolate,
      DOMStringMap::AttributeName(ToUtf8(isolate, property.As<v8::String>())),
      ToAtomicString(isolate, string));
  return Intercepted::kYes;
}

Intercepted DatasetDeleter(Local<Name> property,
                           const PropertyCallbackInfo<Boolean>& info) {
  Isolate* isolate = info.GetIsolate();
  Element* element = static_cast<DOMStringMap*>(
                         static_cast<ScriptWrappable*>(
                             Object::Unwrap<kWrapperTag, Object::Wrappable>(
                                 isolate, info.Holder())))
                         ->element();
  element->RemoveAttribute(isolate, DOMStringMap::AttributeName(ToUtf8(
                                        isolate, property.As<v8::String>())));
  info.GetReturnValue().Set(true);
  return Intercepted::kYes;
}

void DatasetEnumerator(const PropertyCallbackInfo<Array>& info) {
  Isolate* isolate = info.GetIsolate();
  Element* element = static_cast<DOMStringMap*>(
                         static_cast<ScriptWrappable*>(
                             Object::Unwrap<kWrapperTag, Object::Wrappable>(
                                 isolate, info.Holder())))
                         ->element();
  element->SynchronizeAttributes();
  LocalVector<Value> names(isolate);
  for (const Attribute& attribute : element->attributes()) {
    std::string name = DOMStringMap::PropertyName(attribute.name.ToUtf8());
    if (!name.empty()) names.push_back(ToV8StringFromUtf8(isolate, name));
  }
  info.GetReturnValue().Set(Array::New(isolate, names.data(), names.size()));
}

// --- Geometry ----------------------------------------------------------------

void InstallRectBindings(Isolate* isolate) {
  const Interface kRect = Interface::kDOMRect;
  struct Member {
    const char* name;
    double (*get)(DOMRect*);
  };
  static const Member kMembers[] = {
      {"x", [](DOMRect* r) { return r->x; }},
      {"y", [](DOMRect* r) { return r->y; }},
      {"width", [](DOMRect* r) { return r->width; }},
      {"height", [](DOMRect* r) { return r->height; }},
      {"left", [](DOMRect* r) { return r->x; }},
      {"top", [](DOMRect* r) { return r->y; }},
      {"right", [](DOMRect* r) { return r->x + r->width; }},
      {"bottom", [](DOMRect* r) { return r->y + r->height; }},
  };
  for (size_t i = 0; i < arraysize(kMembers); ++i) {
    DefineAccessor(
        isolate, kRect, kMembers[i].name,
        [](const CallbackInfo& info) {
          int index = DataInt(info);
          info.GetReturnValue().Set(
              kMembers[index].get(Receiver<DOMRect>(info)));
        },
        nullptr, Integer::New(isolate, static_cast<int>(i)));
  }
  DefineMethod(isolate, kRect, "toJSON", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    DOMRect* rect = Receiver<DOMRect>(info);
    Local<Object> json = Object::New(isolate);
    for (const Member& member : kMembers) {
      USE(json->Set(context, V8Name(isolate, member.name),
                    Number::New(isolate, member.get(rect))));
    }
    info.GetReturnValue().Set(json);
  });
}

DOMRect* RectOf(Isolate* isolate, Element* element) {
  LayoutRect rect = BoundingClientRect(element);
  return MakeGarbageCollected<DOMRect>(isolate, rect.x, rect.y, rect.width,
                                       rect.height);
}

// --- Element -----------------------------------------------------------------

CALLBACK(ElementInsertAdjacent) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 2)) return;
  Element* element = Receiver<Element>(info);
  int kind = DataInt(info);  // 0 HTML, 1 element, 2 text
  std::string where;
  if (!ToStdString(info, info[0], &where)) return;
  where = AsciiLower(where);

  ContainerNode* parent;
  Node* reference;
  Element* context;
  if (where == "beforebegin") {
    parent = element->parent();
    reference = element;
    context = element->parent_element();
  } else if (where == "afterbegin") {
    parent = element;
    reference = element->first();
    context = element;
  } else if (where == "beforeend") {
    parent = element;
    reference = nullptr;
    context = element;
  } else if (where == "afterend") {
    parent = element->parent();
    reference = element->next_sibling();
    context = element->parent_element();
  } else {
    ThrowDOMException(isolate, "SyntaxError",
                      "The value provided ('" + where +
                          "') is not one of 'beforeBegin', 'afterBegin', "
                          "'beforeEnd', or 'afterEnd'.");
    return;
  }
  if (!parent) {
    if (kind == 0) {
      ThrowDOMException(isolate, "NoModificationAllowedError",
                        "The element has no parent.");
    }
    return;
  }
  CEReactionsScope reactions(isolate);
  Node* node;
  if (kind == 0) {
    std::string html;
    if (!ToStdString(info, info[1], &html)) return;
    DocumentFragment* fragment =
        element->document()->CreateDocumentFragment(isolate);
    ParseHTMLFragment(isolate, fragment, context, html);
    node = fragment;
  } else if (kind == 1) {
    node = To<Element>(isolate, info[1], Interface::kElement);
    if (!node) {
      ThrowTypeError(isolate, "parameter 2 is not of type 'Element'.");
      return;
    }
  } else {
    String text;
    if (!ToString(info, info[1], &text)) return;
    node = element->document()->CreateTextNode(isolate, text);
  }
  ExceptionState exception_state;
  parent->InsertBefore(isolate, node, reference, &exception_state);
  if (ThrowIfFailed(isolate, exception_state)) return;
  if (kind == 1) info.GetReturnValue().Set(info[1]);
}

CALLBACK(ElementAttachShadow) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Element* element = Receiver<Element>(info);
  if (!info[0]->IsObject()) {
    ThrowTypeError(isolate, "parameter 1 is not of type 'ShadowRootInit'.");
    return;
  }
  Local<Value> mode_value;
  std::string mode;
  if (!GetProperty(isolate, info[0].As<Object>(), "mode", &mode_value) ||
      !ToStdString(info, mode_value, &mode)) {
    return;
  }
  if (mode != "open" && mode != "closed") {
    ThrowTypeError(isolate,
                   "The provided value '" + mode +
                       "' is not a valid enum value of type ShadowRootMode.");
    return;
  }
  if (element->shadow_root()) {
    ThrowDOMException(isolate, "NotSupportedError",
                      "Shadow root cannot be created on a host which already "
                      "hosts a shadow tree.");
    return;
  }
  Return(info, element->AttachShadow(isolate, mode == "open"));
}

template <bool matches>
CALLBACK(ElementMatchesOrClosest) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Element* element = Receiver<Element>(info);
  AtomicString selectors;
  if (!ToAtom(info, info[0], &selectors)) return;
  ExceptionState exception_state;
  if (matches) {
    bool result = element->Matches(isolate, selectors, &exception_state);
    if (ThrowIfFailed(isolate, exception_state)) return;
    info.GetReturnValue().Set(result);
  } else {
    Element* result = element->Closest(isolate, selectors, &exception_state);
    if (ThrowIfFailed(isolate, exception_state)) return;
    Return(info, result);
  }
}

void InstallElementCoreBindings(Isolate* isolate) {
  const Interface kElement = Interface::kElement;
  DefineAccessor(isolate, kElement, "tagName", [](const CallbackInfo& info) {
    Return(info, Receiver<Element>(info)->TagName());
  });
  DefineAccessor(isolate, kElement, "localName", [](const CallbackInfo& info) {
    Return(info, Receiver<Element>(info)->local_name());
  });
  DefineAccessor(
      isolate, kElement, "namespaceURI", [](const CallbackInfo& info) {
        const char* uri = NamespaceURI(Receiver<Element>(info)->ns());
        if (!uri) {
          info.GetReturnValue().SetNull();
        } else {
          Return(info, AtomicString(uri));
        }
      });
  DefineAccessor(isolate, kElement, "prefix", [](const CallbackInfo& info) {
    info.GetReturnValue().SetNull();
  });
  DefineAccessor(isolate, kElement, "shadowRoot", [](const CallbackInfo& info) {
    ShadowRoot* root = Receiver<Element>(info)->shadow_root();
    Return(info, root && root->open() ? root : nullptr);
  });
  DefineAccessor(
      isolate, kElement, "innerHTML",
      [](const CallbackInfo& info) {
        Return(info, Receiver<Element>(info)->InnerHTML());
      },
      [](const CallbackInfo& info) {
        String html;
        if (!ToStringNullAsEmpty(info, info[0], &html)) return;
        Receiver<Element>(info)->SetInnerHTML(info.GetIsolate(), html);
      });
  DefineAccessor(
      isolate, kElement, "outerHTML",
      [](const CallbackInfo& info) {
        Return(info, Receiver<Element>(info)->OuterHTML());
      },
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        Element* element = Receiver<Element>(info);
        std::string html;
        if (!info[0]->IsNull() && !ToStdString(info, info[0], &html)) return;
        ContainerNode* parent = element->parent();
        if (!parent) return;
        CEReactionsScope reactions(isolate);
        DocumentFragment* fragment =
            element->document()->CreateDocumentFragment(isolate);
        ParseHTMLFragment(isolate, fragment, element->parent_element(), html);
        ExceptionState exception_state;
        parent->ReplaceChild(isolate, fragment, element, &exception_state);
        ThrowIfFailed(isolate, exception_state);
      });
  DefineMethod(isolate, kElement, "matches", ElementMatchesOrClosest<true>, 1);
  DefineMethod(isolate, kElement, "webkitMatchesSelector",
               ElementMatchesOrClosest<true>, 1);
  DefineMethod(isolate, kElement, "closest", ElementMatchesOrClosest<false>, 1);
  DefineMethod(isolate, kElement, "insertAdjacentHTML", ElementInsertAdjacent,
               2, Integer::New(isolate, 0));
  DefineMethod(isolate, kElement, "insertAdjacentElement",
               ElementInsertAdjacent, 2, Integer::New(isolate, 1));
  DefineMethod(isolate, kElement, "insertAdjacentText", ElementInsertAdjacent,
               2, Integer::New(isolate, 2));
  DefineMethod(isolate, kElement, "attachShadow", ElementAttachShadow, 1);

  // Geometry and scrolling.
  DefineMethod(
      isolate, kElement, "getBoundingClientRect", [](const CallbackInfo& info) {
        Return(info, RectOf(info.GetIsolate(), Receiver<Element>(info)));
      });
  DefineMethod(isolate, kElement, "getClientRects",
               [](const CallbackInfo& info) {
                 Isolate* isolate = info.GetIsolate();
                 Local<Value> rect =
                     ToV8(isolate, RectOf(isolate, Receiver<Element>(info)));
                 info.GetReturnValue().Set(Array::New(isolate, &rect, 1));
               });
  struct Metric {
    const char* name;
    double (*get)(Element*);
  };
  static const Metric kMetrics[] = {
      {"clientWidth", [](Element* e) { return LayoutBoxFor(e).client_width; }},
      {"clientHeight",
       [](Element* e) { return LayoutBoxFor(e).client_height; }},
      {"clientTop", [](Element* e) { return LayoutBoxFor(e).border_top; }},
      {"clientLeft", [](Element* e) { return LayoutBoxFor(e).border_left; }},
      {"scrollWidth", [](Element* e) { return LayoutBoxFor(e).scroll_width; }},
      {"scrollHeight",
       [](Element* e) { return LayoutBoxFor(e).scroll_height; }},
  };
  for (size_t i = 0; i < arraysize(kMetrics); ++i) {
    DefineAccessor(
        isolate, kElement, kMetrics[i].name,
        [](const CallbackInfo& info) {
          int index = DataInt(info);
          info.GetReturnValue().Set(
              std::round(kMetrics[index].get(Receiver<Element>(info))));
        },
        nullptr, Integer::New(isolate, static_cast<int>(i)));
  }
  DefineAccessor(
      isolate, kElement, "scrollTop",
      [](const CallbackInfo& info) {
        ElementRareData* rare = Receiver<Element>(info)->rare_data();
        info.GetReturnValue().Set(rare ? rare->scroll_top : 0.0);
      },
      [](const CallbackInfo& info) {
        double value;
        if (!ToNumber(info, info[0], &value)) return;
        Receiver<Element>(info)->EnsureRareData(info.GetIsolate())->scroll_top =
            value < 0 || std::isnan(value) ? 0 : value;
      });
  DefineAccessor(
      isolate, kElement, "scrollLeft",
      [](const CallbackInfo& info) {
        ElementRareData* rare = Receiver<Element>(info)->rare_data();
        info.GetReturnValue().Set(rare ? rare->scroll_left : 0.0);
      },
      [](const CallbackInfo& info) {
        double value;
        if (!ToNumber(info, info[0], &value)) return;
        Receiver<Element>(info)
            ->EnsureRareData(info.GetIsolate())
            ->scroll_left = value < 0 || std::isnan(value) ? 0 : value;
      });
  // Scrolling and pointer capture have no observable effect without layout
  // and input devices.
  for (const char* name :
       {"scrollIntoView", "scrollIntoViewIfNeeded", "scrollTo", "scroll",
        "scrollBy", "setPointerCapture", "releasePointerCapture"}) {
    DefineMethod(isolate, kElement, name, [](const CallbackInfo& info) {});
  }
  DefineMethod(
      isolate, kElement, "hasPointerCapture",
      [](const CallbackInfo& info) { info.GetReturnValue().Set(false); });
}

// --- Reflected attributes ----------------------------------------------------

enum class Reflect { kString, kBool, kInt, kURL };

struct ReflectedAttribute {
  Interface interface;
  const char* property;
  const char* attribute;
  Reflect type;
  const char* default_value;
};

const ReflectedAttribute kReflectedAttributes[] = {
    {Interface::kElement, "id", "id", Reflect::kString, ""},
    {Interface::kElement, "className", "class", Reflect::kString, ""},
    {Interface::kElement, "slot", "slot", Reflect::kString, ""},
    {Interface::kHTMLElement, "title", "title", Reflect::kString, ""},
    {Interface::kHTMLElement, "lang", "lang", Reflect::kString, ""},
    {Interface::kHTMLElement, "dir", "dir", Reflect::kString, ""},
    {Interface::kHTMLElement, "hidden", "hidden", Reflect::kBool, ""},
    {Interface::kHTMLElement, "accessKey", "accesskey", Reflect::kString, ""},
    {Interface::kHTMLElement, "draggable", "draggable", Reflect::kBool, ""},
    {Interface::kHTMLElement, "inert", "inert", Reflect::kBool, ""},
    {Interface::kHTMLElement, "autofocus", "autofocus", Reflect::kBool, ""},
    {Interface::kHTMLElement, "nonce", "nonce", Reflect::kString, ""},
    {Interface::kHTMLElement, "role", "role", Reflect::kString, ""},
    {Interface::kHTMLElement, "tabIndex", "tabindex", Reflect::kInt, "-1"},
    {Interface::kHTMLElement, "contentEditable", "contenteditable",
     Reflect::kString, "inherit"},
    {Interface::kHTMLElement, "spellcheck", "spellcheck", Reflect::kBool, ""},
    {Interface::kHTMLInputElement, "name", "name", Reflect::kString, ""},
    {Interface::kHTMLInputElement, "placeholder", "placeholder",
     Reflect::kString, ""},
    {Interface::kHTMLInputElement, "disabled", "disabled", Reflect::kBool, ""},
    {Interface::kHTMLInputElement, "readOnly", "readonly", Reflect::kBool, ""},
    {Interface::kHTMLInputElement, "required", "required", Reflect::kBool, ""},
    {Interface::kHTMLInputElement, "multiple", "multiple", Reflect::kBool, ""},
    {Interface::kHTMLInputElement, "min", "min", Reflect::kString, ""},
    {Interface::kHTMLInputElement, "max", "max", Reflect::kString, ""},
    {Interface::kHTMLInputElement, "step", "step", Reflect::kString, ""},
    {Interface::kHTMLInputElement, "pattern", "pattern", Reflect::kString, ""},
    {Interface::kHTMLInputElement, "accept", "accept", Reflect::kString, ""},
    {Interface::kHTMLInputElement, "alt", "alt", Reflect::kString, ""},
    {Interface::kHTMLInputElement, "autocomplete", "autocomplete",
     Reflect::kString, ""},
    {Interface::kHTMLInputElement, "maxLength", "maxlength", Reflect::kInt,
     "-1"},
    {Interface::kHTMLInputElement, "size", "size", Reflect::kInt, "20"},
    {Interface::kHTMLInputElement, "src", "src", Reflect::kURL, ""},
    {Interface::kHTMLInputElement, "defaultValue", "value", Reflect::kString,
     ""},
    {Interface::kHTMLInputElement, "defaultChecked", "checked", Reflect::kBool,
     ""},
    {Interface::kHTMLButtonElement, "name", "name", Reflect::kString, ""},
    {Interface::kHTMLButtonElement, "value", "value", Reflect::kString, ""},
    {Interface::kHTMLButtonElement, "disabled", "disabled", Reflect::kBool, ""},
    {Interface::kHTMLTextAreaElement, "name", "name", Reflect::kString, ""},
    {Interface::kHTMLTextAreaElement, "placeholder", "placeholder",
     Reflect::kString, ""},
    {Interface::kHTMLTextAreaElement, "disabled", "disabled", Reflect::kBool,
     ""},
    {Interface::kHTMLTextAreaElement, "readOnly", "readonly", Reflect::kBool,
     ""},
    {Interface::kHTMLTextAreaElement, "required", "required", Reflect::kBool,
     ""},
    {Interface::kHTMLTextAreaElement, "rows", "rows", Reflect::kInt, "2"},
    {Interface::kHTMLTextAreaElement, "cols", "cols", Reflect::kInt, "20"},
    {Interface::kHTMLSelectElement, "name", "name", Reflect::kString, ""},
    {Interface::kHTMLSelectElement, "disabled", "disabled", Reflect::kBool, ""},
    {Interface::kHTMLSelectElement, "multiple", "multiple", Reflect::kBool, ""},
    {Interface::kHTMLSelectElement, "required", "required", Reflect::kBool, ""},
    {Interface::kHTMLOptionElement, "disabled", "disabled", Reflect::kBool, ""},
    {Interface::kHTMLOptionElement, "label", "label", Reflect::kString, ""},
    {Interface::kHTMLOptionElement, "defaultSelected", "selected",
     Reflect::kBool, ""},
    {Interface::kHTMLFieldSetElement, "disabled", "disabled", Reflect::kBool,
     ""},
    {Interface::kHTMLLabelElement, "htmlFor", "for", Reflect::kString, ""},
    {Interface::kHTMLAnchorElement, "href", "href", Reflect::kURL, ""},
    {Interface::kHTMLAnchorElement, "target", "target", Reflect::kString, ""},
    {Interface::kHTMLAnchorElement, "rel", "rel", Reflect::kString, ""},
    {Interface::kHTMLAnchorElement, "download", "download", Reflect::kString,
     ""},
    {Interface::kHTMLAnchorElement, "type", "type", Reflect::kString, ""},
    {Interface::kHTMLImageElement, "src", "src", Reflect::kURL, ""},
    {Interface::kHTMLImageElement, "alt", "alt", Reflect::kString, ""},
    {Interface::kHTMLImageElement, "srcset", "srcset", Reflect::kString, ""},
    {Interface::kHTMLImageElement, "sizes", "sizes", Reflect::kString, ""},
    {Interface::kHTMLImageElement, "loading", "loading", Reflect::kString,
     "eager"},
    {Interface::kHTMLImageElement, "decoding", "decoding", Reflect::kString,
     "auto"},
    {Interface::kHTMLImageElement, "crossOrigin", "crossorigin",
     Reflect::kString, ""},
    {Interface::kHTMLImageElement, "width", "width", Reflect::kInt, "0"},
    {Interface::kHTMLImageElement, "height", "height", Reflect::kInt, "0"},
    {Interface::kHTMLLinkElement, "href", "href", Reflect::kURL, ""},
    {Interface::kHTMLLinkElement, "rel", "rel", Reflect::kString, ""},
    {Interface::kHTMLLinkElement, "as", "as", Reflect::kString, ""},
    {Interface::kHTMLLinkElement, "type", "type", Reflect::kString, ""},
    {Interface::kHTMLLinkElement, "media", "media", Reflect::kString, ""},
    {Interface::kHTMLLinkElement, "crossOrigin", "crossorigin",
     Reflect::kString, ""},
    {Interface::kHTMLLinkElement, "integrity", "integrity", Reflect::kString,
     ""},
    {Interface::kHTMLLinkElement, "disabled", "disabled", Reflect::kBool, ""},
    {Interface::kHTMLScriptElement, "src", "src", Reflect::kURL, ""},
    {Interface::kHTMLScriptElement, "type", "type", Reflect::kString, ""},
    {Interface::kHTMLScriptElement, "async", "async", Reflect::kBool, ""},
    {Interface::kHTMLScriptElement, "defer", "defer", Reflect::kBool, ""},
    {Interface::kHTMLScriptElement, "noModule", "nomodule", Reflect::kBool, ""},
    {Interface::kHTMLScriptElement, "crossOrigin", "crossorigin",
     Reflect::kString, ""},
    {Interface::kHTMLScriptElement, "charset", "charset", Reflect::kString, ""},
    {Interface::kHTMLScriptElement, "integrity", "integrity", Reflect::kString,
     ""},
    {Interface::kHTMLStyleElement, "media", "media", Reflect::kString, ""},
    {Interface::kHTMLStyleElement, "type", "type", Reflect::kString, ""},
    {Interface::kHTMLMetaElement, "name", "name", Reflect::kString, ""},
    {Interface::kHTMLMetaElement, "content", "content", Reflect::kString, ""},
    {Interface::kHTMLMetaElement, "httpEquiv", "http-equiv", Reflect::kString,
     ""},
    {Interface::kHTMLFormElement, "action", "action", Reflect::kURL, ""},
    {Interface::kHTMLFormElement, "method", "method", Reflect::kString, "get"},
    {Interface::kHTMLFormElement, "name", "name", Reflect::kString, ""},
    {Interface::kHTMLFormElement, "noValidate", "novalidate", Reflect::kBool,
     ""},
    {Interface::kHTMLFormElement, "target", "target", Reflect::kString, ""},
    {Interface::kHTMLCanvasElement, "width", "width", Reflect::kInt, "300"},
    {Interface::kHTMLCanvasElement, "height", "height", Reflect::kInt, "150"},
    {Interface::kHTMLSlotElement, "name", "name", Reflect::kString, ""},
    {Interface::kHTMLIFrameElement, "src", "src", Reflect::kURL, ""},
    {Interface::kHTMLIFrameElement, "name", "name", Reflect::kString, ""},
    {Interface::kHTMLIFrameElement, "width", "width", Reflect::kString, ""},
    {Interface::kHTMLIFrameElement, "height", "height", Reflect::kString, ""},
    {Interface::kHTMLIFrameElement, "allow", "allow", Reflect::kString, ""},
    {Interface::kHTMLLIElement, "value", "value", Reflect::kInt, "0"},
    {Interface::kHTMLOListElement, "start", "start", Reflect::kInt, "1"},
    {Interface::kHTMLDetailsElement, "open", "open", Reflect::kBool, ""},
    {Interface::kHTMLDialogElement, "open", "open", Reflect::kBool, ""},
    {Interface::kHTMLTimeElement, "dateTime", "datetime", Reflect::kString, ""},
    {Interface::kHTMLTableCellElement, "colSpan", "colspan", Reflect::kInt,
     "1"},
    {Interface::kHTMLTableCellElement, "rowSpan", "rowspan", Reflect::kInt,
     "1"},
    {Interface::kHTMLSourceElement, "src", "src", Reflect::kURL, ""},
    {Interface::kHTMLSourceElement, "srcset", "srcset", Reflect::kString, ""},
    {Interface::kHTMLSourceElement, "type", "type", Reflect::kString, ""},
    {Interface::kHTMLSourceElement, "media", "media", Reflect::kString, ""},
};

CALLBACK(ReflectedGetter) {
  Isolate* isolate = info.GetIsolate();
  const ReflectedAttribute& reflected = kReflectedAttributes[DataInt(info)];
  Element* element = Receiver<Element>(info);
  const AtomicString& value =
      element->GetAttribute(AtomicString(reflected.attribute));
  switch (reflected.type) {
    case Reflect::kString:
      if (value.is_null()) {
        Return(info, AtomicString(reflected.default_value));
      } else {
        Return(info, value);
      }
      return;
    case Reflect::kBool:
      info.GetReturnValue().Set(!value.is_null());
      return;
    case Reflect::kInt: {
      std::string text =
          value.is_null() ? reflected.default_value : value.ToUtf8();
      char* end = nullptr;
      long parsed = std::strtol(text.c_str(), &end, 10);
      if (end == text.c_str()) parsed = std::atol(reflected.default_value);
      info.GetReturnValue().Set(static_cast<int>(parsed));
      return;
    }
    case Reflect::kURL: {
      if (value.is_null()) {
        Return(info, String::Empty());
        return;
      }
      ParsedURL url = ParsedURL::Resolve(
          value.ToUtf8(), element->WrapperWindow()->location()->url());
      info.GetReturnValue().Set(
          ToV8StringFromUtf8(isolate, url.valid ? url.Href() : value.ToUtf8()));
      return;
    }
  }
}

CALLBACK(ReflectedSetter) {
  Isolate* isolate = info.GetIsolate();
  const ReflectedAttribute& reflected = kReflectedAttributes[DataInt(info)];
  Element* element = Receiver<Element>(info);
  AtomicString name(reflected.attribute);
  switch (reflected.type) {
    case Reflect::kString:
    case Reflect::kURL: {
      AtomicString value;
      if (!ToAtom(info, info[0], &value)) return;
      element->SetAttribute(isolate, name, value);
      return;
    }
    case Reflect::kBool:
      if (info[0]->BooleanValue(isolate)) {
        element->SetAttribute(isolate, name, AtomicString::EmptyAtom());
      } else {
        element->RemoveAttribute(isolate, name);
      }
      return;
    case Reflect::kInt: {
      int value;
      if (!ToInt(info, info[0], &value)) return;
      element->SetAttribute(
          isolate, name, AtomicString(std::string_view(std::to_string(value))));
      return;
    }
  }
}

// --- Event handler IDL attributes -------------------------------------------

const char* const kElementEventHandlers[] = {
    "abort",
    "animationend",
    "animationiteration",
    "animationstart",
    "beforeinput",
    "blur",
    "cancel",
    "change",
    "click",
    "close",
    "contextmenu",
    "copy",
    "cut",
    "dblclick",
    "drag",
    "dragend",
    "dragenter",
    "dragleave",
    "dragover",
    "dragstart",
    "drop",
    "error",
    "focus",
    "focusin",
    "focusout",
    "input",
    "invalid",
    "keydown",
    "keypress",
    "keyup",
    "load",
    "mousedown",
    "mouseenter",
    "mouseleave",
    "mousemove",
    "mouseout",
    "mouseover",
    "mouseup",
    "paste",
    "pointercancel",
    "pointerdown",
    "pointerenter",
    "pointerleave",
    "pointermove",
    "pointerout",
    "pointerover",
    "pointerup",
    "reset",
    "resize",
    "scroll",
    "select",
    "selectionchange",
    "selectstart",
    "submit",
    "toggle",
    "touchcancel",
    "touchend",
    "touchmove",
    "touchstart",
    "transitionend",
    "wheel",
    "readystatechange",
    "slotchange",
};

CALLBACK(EventHandlerGetter) {
  Isolate* isolate = info.GetIsolate();
  info.GetReturnValue().Set(Receiver<EventTarget>(info)->GetAttributeListener(
      isolate, ToAtomicString(isolate, info.Data().As<v8::String>())));
}

CALLBACK(EventHandlerSetter) {
  Isolate* isolate = info.GetIsolate();
  Receiver<EventTarget>(info)->SetAttributeListener(
      isolate, ToAtomicString(isolate, info.Data().As<v8::String>()), info[0]);
}

void InstallEventHandlers(Isolate* isolate, Interface interface) {
  for (const char* type : kElementEventHandlers) {
    std::string property = std::string("on") + type;
    DefineAccessor(isolate, interface, property.c_str(), EventHandlerGetter,
                   EventHandlerSetter, V8Name(isolate, type));
  }
}

// --- HTMLElement -------------------------------------------------------------

CALLBACK(HTMLElementConstructor) {
  Isolate* isolate = info.GetIsolate();
  if (!info.IsConstructCall()) {
    ThrowTypeError(isolate,
                   "Please use the 'new' operator, this DOM object "
                   "constructor cannot be called as a function.");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Window* window = Window::From(context);
  CustomElementDefinition* definition =
      window->custom_elements()->LookupByConstructor(isolate, info.NewTarget());
  if (!definition) {
    ThrowTypeError(isolate, "Illegal constructor");
    return;
  }
  if (definition->construction_stack.empty()) {
    // `new MyElement()` or document.createElement: make a fresh element and
    // adopt the object V8 already allocated with the right prototype.
    Element* element = window->document()->CreateElement(
        isolate, Namespace::kHTML, definition->name);
    element->set_flag(Node::kIsUndefinedCustom, false);
    element->set_flag(Node::kIsCustom, true);
    element->EnsureRareData(isolate)->definition = definition;
    Object::Wrap<kWrapperTag>(isolate, info.This(), element);
    element->set_wrapper(isolate, info.This());
    return;
  }
  // Upgrade: hand back the existing element with its prototype switched to
  // the custom class.
  Element* element = definition->construction_stack.back().Get();
  if (!element) {
    ThrowDOMException(isolate, "InvalidStateError",
                      "This custom element has already been constructed.");
    return;
  }
  definition->construction_stack.back().Clear();
  Local<Object> wrapper = ToV8(isolate, element).As<Object>();
  Local<Value> prototype;
  if (!info.NewTarget()
           .As<Object>()
           ->Get(context, V8Name(isolate, "prototype"))
           .ToLocal(&prototype)) {
    return;
  }
  if (wrapper->SetPrototype(context, prototype).IsNothing()) return;
  info.GetReturnValue().Set(wrapper);
}

template <Interface interface>
void InstallStyleAndDataset(Isolate* isolate) {
  DefineAccessor(
      isolate, interface, "style",
      [](const CallbackInfo& info) {
        Return(info, InlineStyle(info.GetIsolate(), Receiver<Element>(info)));
      },
      [](const CallbackInfo& info) {
        // [PutForwards=cssText]
        String text;
        if (!ToStringNullAsEmpty(info, info[0], &text)) return;
        InlineStyle(info.GetIsolate(), Receiver<Element>(info))
            ->SetCssText(info.GetIsolate(), text);
      });
  DefineAccessor(isolate, interface, "dataset", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Element* element = Receiver<Element>(info);
    ElementRareData* rare = element->EnsureRareData(isolate);
    if (!rare->dataset) {
      rare->dataset = MakeGarbageCollected<DOMStringMap>(isolate, element);
    }
    Return(info, rare->dataset.Get());
  });
  DefineMethod(isolate, interface, "focus", [](const CallbackInfo& info) {
    Receiver<Element>(info)->Focus(info.GetIsolate());
  });
  DefineMethod(isolate, interface, "blur", [](const CallbackInfo& info) {
    Receiver<Element>(info)->Blur(info.GetIsolate());
  });
  InstallEventHandlers(isolate, interface);
}

Element* OffsetParent(Element* element) {
  if (element->HasTag("body") || element->HasTag("html")) return nullptr;
  return element->document()->body();
}

void InstallHTMLElementBindings(Isolate* isolate) {
  const Interface kHTMLElement = Interface::kHTMLElement;
  for (int i = 0; i < kInterfaceCount; ++i) {
    Interface interface = static_cast<Interface>(i);
    if (InterfaceInheritsFrom(interface, kHTMLElement)) {
      SetConstructor(isolate, interface, HTMLElementConstructor);
    }
  }
  InstallStyleAndDataset<Interface::kHTMLElement>(isolate);
  InstallStyleAndDataset<Interface::kSVGElement>(isolate);
  // GlobalEventHandlers on Document.
  InstallEventHandlers(isolate, Interface::kDocument);

  DefineMethod(isolate, kHTMLElement, "click", [](const CallbackInfo& info) {
    Receiver<Element>(info)->Click(info.GetIsolate());
  });
  auto inner_text_getter = [](const CallbackInfo& info) {
    Return(info, Receiver<Element>(info)->TextContent());
  };
  auto inner_text_setter = [](const CallbackInfo& info) {
    String text;
    if (!ToStringNullAsEmpty(info, info[0], &text)) return;
    Receiver<Element>(info)->SetTextContent(info.GetIsolate(), text);
  };
  // innerText is approximated by textContent: there is no rendering to
  // derive line breaks and hidden text from.
  DefineAccessor(isolate, kHTMLElement, "innerText", inner_text_getter,
                 inner_text_setter);
  DefineAccessor(isolate, kHTMLElement, "outerText", inner_text_getter);
  DefineAccessor(
      isolate, kHTMLElement, "isContentEditable", [](const CallbackInfo& info) {
        static const AtomicString* attribute =
            new AtomicString("contenteditable");
        for (Element* e = Receiver<Element>(info); e; e = e->parent_element()) {
          const AtomicString& value = e->GetAttribute(*attribute);
          if (value.is_null()) continue;
          info.GetReturnValue().Set(!value.EqualsAscii("false"));
          return;
        }
        info.GetReturnValue().Set(false);
      });
  DefineAccessor(isolate, kHTMLElement, "offsetParent",
                 [](const CallbackInfo& info) {
                   Return(info, OffsetParent(Receiver<Element>(info)));
                 });
  struct Metric {
    const char* name;
    double (*get)(Element*);
  };
  static const Metric kMetrics[] = {
      {"offsetWidth", [](Element* e) { return LayoutBoxFor(e).width; }},
      {"offsetHeight", [](Element* e) { return LayoutBoxFor(e).height; }},
      {"offsetTop",
       [](Element* e) {
         Element* parent = OffsetParent(e);
         return BoundingClientRect(e).y -
                (parent ? BoundingClientRect(parent).y : 0);
       }},
      {"offsetLeft",
       [](Element* e) {
         Element* parent = OffsetParent(e);
         return BoundingClientRect(e).x -
                (parent ? BoundingClientRect(parent).x : 0);
       }},
  };
  for (size_t i = 0; i < arraysize(kMetrics); ++i) {
    DefineAccessor(
        isolate, kHTMLElement, kMetrics[i].name,
        [](const CallbackInfo& info) {
          int index = DataInt(info);
          info.GetReturnValue().Set(
              std::round(kMetrics[index].get(Receiver<Element>(info))));
        },
        nullptr, Integer::New(isolate, static_cast<int>(i)));
  }

  for (size_t i = 0; i < arraysize(kReflectedAttributes); ++i) {
    DefineAccessor(isolate, kReflectedAttributes[i].interface,
                   kReflectedAttributes[i].property, ReflectedGetter,
                   ReflectedSetter, Integer::New(isolate, static_cast<int>(i)));
  }

  InterfaceTemplate(isolate, Interface::kDOMStringMap)
      ->InstanceTemplate()
      ->SetHandler(NamedPropertyHandlerConfiguration(
          DatasetGetter, DatasetSetter, nullptr, DatasetDeleter,
          DatasetEnumerator, Local<Value>(),
          PropertyHandlerFlags::kOnlyInterceptStrings));
}

// --- Form controls -----------------------------------------------------------

Element* FormOf(Element* control) {
  for (Element* e = control->parent_element(); e; e = e->parent_element()) {
    if (e->HasTag("form")) return e;
  }
  return nullptr;
}

std::string InputType(Element* element) {
  static const AtomicString* type_atom = new AtomicString("type");
  static const char* const kKnown[] = {
      "hidden",   "text",  "search", "tel",      "url",   "email",
      "password", "date",  "month",  "week",     "time",  "datetime-local",
      "number",   "range", "color",  "checkbox", "radio", "file",
      "submit",   "image", "reset",  "button"};
  const AtomicString& value = element->GetAttribute(*type_atom);
  if (value.is_null()) return "text";
  std::string lower = AsciiLower(value.ToUtf8());
  for (const char* known : kKnown) {
    if (lower == known) return lower;
  }
  return "text";
}

void CollectOptions(Element* select, std::vector<Element*>* options) {
  for (Node* n = select->first(); n; n = n->NextInPreOrder(select)) {
    if (n->IsElement() && static_cast<Element*>(n)->HasTag("option")) {
      options->push_back(static_cast<Element*>(n));
    }
  }
}

void InstallFormBindings(Isolate* isolate) {
  auto value_getter = [](const CallbackInfo& info) {
    Return(info, Receiver<Element>(info)->Value());
  };
  auto value_setter = [](const CallbackInfo& info) {
    Element* element = Receiver<Element>(info);
    if (element->HasTag("input")) {
      // For these types the value IDL attribute is in mode "default" or
      // "default/on": setting it sets the content attribute.
      static const char* const kAttributeModeTypes[] = {
          "hidden", "submit", "image", "reset", "button", "checkbox", "radio"};
      const std::string type = InputType(element);
      if (std::find(std::begin(kAttributeModeTypes),
                    std::end(kAttributeModeTypes),
                    type) != std::end(kAttributeModeTypes)) {
        AtomicString value = AtomicString::EmptyAtom();
        if (!info[0]->IsNull() && !ToAtom(info, info[0], &value)) return;
        element->SetAttribute(info.GetIsolate(), AtomicString("value"), value);
        return;
      }
    }
    String value;
    if (!ToStringNullAsEmpty(info, info[0], &value)) return;
    element->SetValue(info.GetIsolate(), value);
  };
  auto form_getter = [](const CallbackInfo& info) {
    Return(info, FormOf(Receiver<Element>(info)));
  };
  for (Interface interface :
       {Interface::kHTMLInputElement, Interface::kHTMLTextAreaElement,
        Interface::kHTMLSelectElement}) {
    DefineAccessor(isolate, interface, "value", value_getter, value_setter);
  }
  DefineAccessor(isolate, Interface::kHTMLOptionElement, "value", value_getter,
                 [](const CallbackInfo& info) {
                   AtomicString value;
                   if (!ToAtom(info, info[0], &value)) return;
                   Receiver<Element>(info)->SetAttribute(
                       info.GetIsolate(), AtomicString("value"), value);
                 });
  for (Interface interface :
       {Interface::kHTMLInputElement, Interface::kHTMLTextAreaElement,
        Interface::kHTMLSelectElement, Interface::kHTMLButtonElement,
        Interface::kHTMLOptionElement, Interface::kHTMLLabelElement,
        Interface::kHTMLFieldSetElement}) {
    DefineAccessor(isolate, interface, "form", form_getter);
  }

  const Interface kInput = Interface::kHTMLInputElement;
  DefineAccessor(
      isolate, kInput, "type",
      [](const CallbackInfo& info) {
        info.GetReturnValue().Set(ToV8StringFromUtf8(
            info.GetIsolate(), InputType(Receiver<Element>(info))));
      },
      [](const CallbackInfo& info) {
        AtomicString value;
        if (!ToAtom(info, info[0], &value)) return;
        Receiver<Element>(info)->SetAttribute(info.GetIsolate(),
                                              AtomicString("type"), value);
      });
  DefineAccessor(
      isolate, kInput, "checked",
      [](const CallbackInfo& info) {
        info.GetReturnValue().Set(Receiver<Element>(info)->Checked());
      },
      [](const CallbackInfo& info) {
        Receiver<Element>(info)->SetChecked(
            info.GetIsolate(), info[0]->BooleanValue(info.GetIsolate()));
      });
  DefineAccessor(
      isolate, kInput, "indeterminate",
      [](const CallbackInfo& info) {
        ElementRareData* rare = Receiver<Element>(info)->rare_data();
        info.GetReturnValue().Set(rare && rare->indeterminate);
      },
      [](const CallbackInfo& info) {
        Element* element = Receiver<Element>(info);
        element->EnsureRareData(info.GetIsolate())->indeterminate =
            info[0]->BooleanValue(info.GetIsolate());
        element->DidChangeState();
      });
  DefineAccessor(
      isolate, kInput, "valueAsNumber",
      [](const CallbackInfo& info) {
        std::string value = Receiver<Element>(info)->Value().ToUtf8();
        char* end = nullptr;
        double number = std::strtod(value.c_str(), &end);
        info.GetReturnValue().Set(end == value.c_str() ? std::nan("") : number);
      },
      [](const CallbackInfo& info) {
        String value;
        if (!ToString(info, info[0], &value)) return;
        Receiver<Element>(info)->SetValue(info.GetIsolate(), value);
      });
  DefineAccessor(isolate, kInput, "files", [](const CallbackInfo& info) {
    info.GetReturnValue().SetNull();
  });

  // Text selection inside a control is tracked as a caret at the end of the
  // value; there is no editing UI to move it.
  for (Interface interface : {kInput, Interface::kHTMLTextAreaElement}) {
    auto selection_getter = [](const CallbackInfo& info) {
      info.GetReturnValue().Set(Receiver<Element>(info)->Value().length());
    };
    auto no_op = [](const CallbackInfo& info) {};
    DefineAccessor(isolate, interface, "selectionStart", selection_getter,
                   no_op);
    DefineAccessor(isolate, interface, "selectionEnd", selection_getter, no_op);
    DefineMethod(isolate, interface, "select", no_op);
    DefineMethod(isolate, interface, "setSelectionRange", no_op, 2);
    DefineMethod(
        isolate, interface, "checkValidity",
        [](const CallbackInfo& info) { info.GetReturnValue().Set(true); });
  }
  DefineAccessor(
      isolate, Interface::kHTMLTextAreaElement, "defaultValue",
      [](const CallbackInfo& info) {
        Return(info, Receiver<Element>(info)->TextContent());
      },
      [](const CallbackInfo& info) {
        String value;
        if (!ToString(info, info[0], &value)) return;
        Receiver<Element>(info)->SetTextContent(info.GetIsolate(), value);
      });
  DefineAccessor(
      isolate, Interface::kHTMLTextAreaElement, "type",
      [](const CallbackInfo& info) { Return(info, AtomicString("textarea")); });

  const Interface kButton = Interface::kHTMLButtonElement;
  DefineAccessor(
      isolate, kButton, "type",
      [](const CallbackInfo& info) {
        const AtomicString& type =
            Receiver<Element>(info)->GetAttribute(AtomicString("type"));
        std::string lower = type.is_null() ? "" : AsciiLower(type.ToUtf8());
        if (lower != "button" && lower != "reset") lower = "submit";
        info.GetReturnValue().Set(ToV8StringFromUtf8(info.GetIsolate(), lower));
      },
      [](const CallbackInfo& info) {
        AtomicString value;
        if (!ToAtom(info, info[0], &value)) return;
        Receiver<Element>(info)->SetAttribute(info.GetIsolate(),
                                              AtomicString("type"), value);
      });

  const Interface kSelect = Interface::kHTMLSelectElement;
  DefineAccessor(isolate, kSelect, "type", [](const CallbackInfo& info) {
    Return(info, AtomicString(Receiver<Element>(info)->HasAttribute(
                                  AtomicString("multiple"))
                                  ? "select-multiple"
                                  : "select-one"));
  });
  DefineAccessor(isolate, kSelect, "options", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Return(info, MakeGarbageCollected<NodeList>(
                     isolate, Interface::kHTMLCollection, NodeList::kByTagName,
                     Receiver<Element>(info), AtomicString("option")));
  });
  DefineAccessor(isolate, kSelect, "length", [](const CallbackInfo& info) {
    MOCK_DOM_LOCAL_NODES
    std::vector<Element*> options;
    CollectOptions(Receiver<Element>(info), &options);
    info.GetReturnValue().Set(static_cast<uint32_t>(options.size()));
  });
  DefineAccessor(
      isolate, kSelect, "selectedIndex",
      [](const CallbackInfo& info) {
        MOCK_DOM_LOCAL_NODES
        std::vector<Element*> options;
        CollectOptions(Receiver<Element>(info), &options);
        for (size_t i = 0; i < options.size(); ++i) {
          if (options[i]->Selected()) {
            info.GetReturnValue().Set(static_cast<int>(i));
            return;
          }
        }
        info.GetReturnValue().Set(options.empty() ? -1 : 0);
      },
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        int index;
        if (!ToInt(info, info[0], &index)) return;
        MOCK_DOM_LOCAL_NODES
        std::vector<Element*> options;
        CollectOptions(Receiver<Element>(info), &options);
        for (size_t i = 0; i < options.size(); ++i) {
          ElementRareData* rare = options[i]->EnsureRareData(isolate);
          rare->selected = static_cast<int>(i) == index;
          rare->has_dirty_selected = true;
          options[i]->DidChangeState();
        }
      });

  const Interface kOption = Interface::kHTMLOptionElement;
  DefineAccessor(
      isolate, kOption, "selected",
      [](const CallbackInfo& info) {
        info.GetReturnValue().Set(Receiver<Element>(info)->Selected());
      },
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        Element* option = Receiver<Element>(info);
        ElementRareData* rare = option->EnsureRareData(isolate);
        rare->selected = info[0]->BooleanValue(isolate);
        rare->has_dirty_selected = true;
        option->DidChangeState();
      });
  DefineAccessor(
      isolate, kOption, "text",
      [](const CallbackInfo& info) {
        Return(info, Receiver<Element>(info)->TextContent());
      },
      [](const CallbackInfo& info) {
        String value;
        if (!ToString(info, info[0], &value)) return;
        Receiver<Element>(info)->SetTextContent(info.GetIsolate(), value);
      });
  DefineAccessor(isolate, kOption, "index", [](const CallbackInfo& info) {
    Element* option = Receiver<Element>(info);
    int index = 0;
    for (Element* e = option->previous_element_sibling(); e;
         e = e->previous_element_sibling()) {
      if (e->HasTag("option")) ++index;
    }
    info.GetReturnValue().Set(index);
  });

  DefineAccessor(
      isolate, Interface::kHTMLLabelElement, "control",
      [](const CallbackInfo& info) {
        Element* label = Receiver<Element>(info);
        const AtomicString& id = label->GetAttribute(AtomicString("for"));
        if (!id.is_null()) {
          Return(info, label->document()->GetElementById(id));
          return;
        }
        for (Node* n = label->first(); n; n = n->NextInPreOrder(label)) {
          if (n->IsElement() && static_cast<Element*>(n)->IsFormControl()) {
            Return(info, n);
            return;
          }
        }
        info.GetReturnValue().SetNull();
      });

  const Interface kForm = Interface::kHTMLFormElement;
  DefineAccessor(isolate, kForm, "elements", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Element* form = Receiver<Element>(info);
    MOCK_DOM_LOCAL_NODES
    std::vector<Element*> controls;
    for (Node* n = form->first(); n; n = n->NextInPreOrder(form)) {
      if (!n->IsElement()) continue;
      Element* element = static_cast<Element*>(n);
      if (element->IsFormControl() && !element->HasTag("option")) {
        controls.push_back(element);
      }
    }
    Return(info, NodeList::CreateStatic(isolate, controls));
  });
  DefineMethod(isolate, kForm, "submit", [](const CallbackInfo& info) {
    Receiver<Element>(info)->document()->window()->ReportUnsupported(
        info.GetIsolate(), "form.submit()");
  });
  DefineMethod(isolate, kForm, "requestSubmit", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Element* form = Receiver<Element>(info);
    Event* submit = MakeGarbageCollected<Event>(
        isolate, isolate, Interface::kSubmitEvent, AtomicString("submit"));
    submit->bubbles = true;
    submit->cancelable = true;
    submit->is_trusted = true;
    if (form->DispatchEvent(isolate, submit)) {
      form->document()->window()->ReportUnsupported(
          isolate, "form submission that navigates");
    }
  });
  DefineMethod(isolate, kForm, "reset", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Element* form = Receiver<Element>(info);
    for (Node* n = form->first(); n; n = n->NextInPreOrder(form)) {
      if (!n->IsElement()) continue;
      if (ElementRareData* rare = static_cast<Element*>(n)->rare_data()) {
        rare->has_dirty_value = false;
        rare->has_dirty_checked = false;
        rare->has_dirty_selected = false;
      }
    }
    Event* reset = MakeGarbageCollected<Event>(
        isolate, isolate, Interface::kEvent, AtomicString("reset"));
    reset->bubbles = true;
    reset->cancelable = true;
    form->DispatchEvent(isolate, reset);
  });
}

// --- Other HTML elements -----------------------------------------------------

void InstallMiscElementBindings(Isolate* isolate) {
  DefineAccessor(isolate, Interface::kHTMLTemplateElement, "content",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Element>(info)->TemplateContent(
                                    info.GetIsolate()));
                 });

  // URL decomposition on <a>.
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
      {"hash", [](const ParsedURL& u) { return u.Hash(); }},
      {"origin", [](const ParsedURL& u) { return u.Origin(); }},
  };
  for (size_t i = 0; i < arraysize(kParts); ++i) {
    DefineAccessor(
        isolate, Interface::kHTMLAnchorElement, kParts[i].name,
        [](const CallbackInfo& info) {
          Element* anchor = Receiver<Element>(info);
          const AtomicString& href = anchor->GetAttribute(AtomicString("href"));
          if (href.is_null()) {
            Return(info, String::Empty());
            return;
          }
          ParsedURL url = ParsedURL::Resolve(
              href.ToUtf8(), anchor->WrapperWindow()->location()->url());
          info.GetReturnValue().Set(ToV8StringFromUtf8(
              info.GetIsolate(), kParts[DataInt(info)].get(url)));
        },
        nullptr, Integer::New(isolate, static_cast<int>(i)));
  }
  auto text_getter = [](const CallbackInfo& info) {
    Return(info, Receiver<Element>(info)->TextContent());
  };
  auto text_setter = [](const CallbackInfo& info) {
    String value;
    if (!ToString(info, info[0], &value)) return;
    Receiver<Element>(info)->SetTextContent(info.GetIsolate(), value);
  };
  for (Interface interface :
       {Interface::kHTMLAnchorElement, Interface::kHTMLScriptElement,
        Interface::kHTMLTitleElement}) {
    DefineAccessor(isolate, interface, "text", text_getter, text_setter);
  }

  // Images never load pixels; they are complete with no intrinsic size.
  const Interface kImage = Interface::kHTMLImageElement;
  DefineAccessor(isolate, kImage, "complete", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(true);
  });
  for (const char* name : {"naturalWidth", "naturalHeight"}) {
    DefineAccessor(isolate, kImage, name, [](const CallbackInfo& info) {
      info.GetReturnValue().Set(0);
    });
  }
  DefineAccessor(isolate, kImage, "currentSrc", [](const CallbackInfo& info) {
    Element* image = Receiver<Element>(info);
    const AtomicString& src = image->GetAttribute(AtomicString("src"));
    if (src.is_null()) {
      Return(info, String::Empty());
      return;
    }
    ParsedURL url = ParsedURL::Resolve(
        src.ToUtf8(), image->WrapperWindow()->location()->url());
    info.GetReturnValue().Set(
        ToV8StringFromUtf8(info.GetIsolate(), url.Href()));
  });

  // <iframe>: the nested realm is attached by the page loader.
  const Interface kIFrame = Interface::kHTMLIFrameElement;
  DefineAccessor(isolate, kIFrame, "contentWindow",
                 [](const CallbackInfo& info) {
                   ElementRareData* rare = Receiver<Element>(info)->rare_data();
                   Return(info, rare ? rare->content_window.Get() : nullptr);
                 });
  DefineAccessor(isolate, kIFrame, "contentDocument",
                 [](const CallbackInfo& info) {
                   ElementRareData* rare = Receiver<Element>(info)->rare_data();
                   Window* window = rare ? rare->content_window.Get() : nullptr;
                   Return(info, window ? window->document() : nullptr);
                 });

  for (Interface interface :
       {Interface::kHTMLLinkElement, Interface::kHTMLStyleElement}) {
    DefineAccessor(isolate, interface, "sheet", [](const CallbackInfo& info) {
      Element* element = Receiver<Element>(info);
      Return(info, element->is_connected() ? SheetOf(info.GetIsolate(), element)
                                           : nullptr);
    });
  }
  for (const char* name : {"show", "showModal"}) {
    DefineMethod(isolate, Interface::kHTMLDialogElement, name,
                 [](const CallbackInfo& info) {
                   Receiver<Element>(info)->SetAttribute(
                       info.GetIsolate(), AtomicString("open"),
                       AtomicString::EmptyAtom());
                 });
  }
  DefineMethod(isolate, Interface::kHTMLDialogElement, "close",
               [](const CallbackInfo& info) {
                 Receiver<Element>(info)->RemoveAttribute(info.GetIsolate(),
                                                          AtomicString("open"));
               });
}

#undef CALLBACK

}  // namespace

void InstallElementBindings(Isolate* isolate) {
  InstallElementCoreBindings(isolate);
  InstallAttributeBindings(isolate);
  InstallTokenListBindings(isolate);
  InstallStyleBindings(isolate);
  InstallRectBindings(isolate);
  InstallHTMLElementBindings(isolate);
  InstallFormBindings(isolate);
  InstallMiscElementBindings(isolate);
}

}  // namespace mock_dom
}  // namespace v8
