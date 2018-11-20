// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CreateSymbol) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  RUNTIME_ASSERT(name->IsString() || name->IsUndefined());
  Handle<Symbol> symbol = isolate->factory()->NewSymbol();
  if (name->IsString()) symbol->set_name(*name);
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_CreatePrivateSymbol) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  RUNTIME_ASSERT(name->IsString() || name->IsUndefined());
  Handle<Symbol> symbol = isolate->factory()->NewPrivateSymbol();
  if (name->IsString()) symbol->set_name(*name);
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_CreatePrivateOwnSymbol) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  RUNTIME_ASSERT(name->IsString() || name->IsUndefined());
  Handle<Symbol> symbol = isolate->factory()->NewPrivateOwnSymbol();
  if (name->IsString()) symbol->set_name(*name);
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_CreateGlobalPrivateOwnSymbol) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  Handle<JSObject> registry = isolate->GetSymbolRegistry();
  Handle<String> part = isolate->factory()->private_intern_string();
  Handle<Object> privates;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, privates, Object::GetPropertyOrElement(registry, part));
  Handle<Object> symbol;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, symbol, Object::GetPropertyOrElement(privates, name));
  if (!symbol->IsSymbol()) {
    DCHECK(symbol->IsUndefined());
    symbol = isolate->factory()->NewPrivateSymbol();
    Handle<Symbol>::cast(symbol)->set_name(*name);
    Handle<Symbol>::cast(symbol)->set_is_own(true);
    JSObject::SetProperty(Handle<JSObject>::cast(privates), name, symbol,
                          STRICT).Assert();
  }
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_NewSymbolWrapper) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Symbol, symbol, 0);
  return *Object::ToObject(isolate, symbol).ToHandleChecked();
}


RUNTIME_FUNCTION(Runtime_SymbolDescription) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Symbol, symbol, 0);
  return symbol->name();
}


RUNTIME_FUNCTION(Runtime_SymbolRegistry) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  return *isolate->GetSymbolRegistry();
}


RUNTIME_FUNCTION(Runtime_SymbolIsPrivate) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Symbol, symbol, 0);
  return isolate->heap()->ToBoolean(symbol->is_private());
}
}
}  // namespace v8::internal
