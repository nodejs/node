// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments-inl.h"
#include "src/isolate-inl.h"
#include "src/objects-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/string-builder-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CreatePrivateSymbol) {
  HandleScope scope(isolate);
  DCHECK_GE(1, args.length());
  Handle<Symbol> symbol = isolate->factory()->NewPrivateSymbol();
  if (args.length() == 1) {
    CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
    CHECK(name->IsString() || name->IsUndefined(isolate));
    if (name->IsString()) symbol->set_name(*name);
  }
  return *symbol;
}

RUNTIME_FUNCTION(Runtime_CreatePrivateFieldSymbol) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  Handle<Symbol> symbol = isolate->factory()->NewPrivateFieldSymbol();
  return *symbol;
}

RUNTIME_FUNCTION(Runtime_SymbolDescriptiveString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Symbol, symbol, 0);
  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("Symbol(");
  if (symbol->name()->IsString()) {
    builder.AppendString(handle(String::cast(symbol->name()), isolate));
  }
  builder.AppendCharacter(')');
  RETURN_RESULT_OR_FAILURE(isolate, builder.Finish());
}


RUNTIME_FUNCTION(Runtime_SymbolIsPrivate) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Symbol, symbol, 0);
  return isolate->heap()->ToBoolean(symbol->is_private());
}
}  // namespace internal
}  // namespace v8
