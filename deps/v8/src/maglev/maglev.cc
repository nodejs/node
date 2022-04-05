// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev.h"

#include "src/common/globals.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compiler.h"

namespace v8 {
namespace internal {

MaybeHandle<CodeT> Maglev::Compile(Isolate* isolate,
                                   Handle<JSFunction> function) {
  DCHECK(FLAG_maglev);
  auto info = maglev::MaglevCompilationInfo::New(isolate, function);
  maglev::MaglevCompilationUnit* const unit = info->toplevel_compilation_unit();
  // TODO(v8:7700): Support exceptions in maglev. We currently bail if exception
  // handler table is non-empty.
  if (unit->bytecode().handler_table_size() > 0) return {};
  maglev::MaglevCompiler::Compile(unit);
  return maglev::MaglevCompiler::GenerateCode(unit);
}

}  // namespace internal
}  // namespace v8
