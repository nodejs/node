// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/arguments-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/strings/string-builder-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CreatePrivateSymbol) {
  HandleScope scope(isolate);
  DCHECK_GE(1, args.length());
  Handle<Symbol> symbol = isolate->factory()->NewPrivateSymbol();
  if (args.length() == 1) {
    CONVERT_ARG_HANDLE_CHECKED(Object, description, 0);
    CHECK(description->IsString() || description->IsUndefined(isolate));
    if (description->IsString())
      symbol->set_description(String::cast(*description));
  }
  return *symbol;
}

RUNTIME_FUNCTION(Runtime_CreatePrivateBrandSymbol) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  Handle<Symbol> symbol = isolate->factory()->NewPrivateNameSymbol(name);
  symbol->set_is_private_brand();
  return *symbol;
}

RUNTIME_FUNCTION(Runtime_CreatePrivateNameSymbol) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  Handle<Symbol> symbol = isolate->factory()->NewPrivateNameSymbol(name);
  return *symbol;
}

RUNTIME_FUNCTION(Runtime_SymbolDescriptiveString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Symbol, symbol, 0);
  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("Symbol(");
  if (symbol->description().IsString()) {
    builder.AppendString(handle(String::cast(symbol->description()), isolate));
  }
  builder.AppendCharacter(')');
  RETURN_RESULT_OR_FAILURE(isolate, builder.Finish());
}


RUNTIME_FUNCTION(Runtime_SymbolIsPrivate) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Symbol, symbol, 0);
  return isolate->heap()->ToBoolean(symbol.is_private());
}
}  // namespace internal
}  // namespace v8
