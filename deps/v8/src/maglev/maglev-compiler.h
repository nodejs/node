// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_COMPILER_H_
#define V8_MAGLEV_MAGLEV_COMPILER_H_

#include "src/common/globals.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/heap-refs.h"
#include "src/maglev/maglev-compilation-unit.h"

namespace v8 {
namespace internal {

namespace compiler {
class JSHeapBroker;
}

namespace maglev {

class Graph;

class MaglevCompiler {
 public:
  // May be called from any thread.
  static void Compile(LocalIsolate* local_isolate,
                      MaglevCompilationUnit* toplevel_compilation_unit);

  // Called on the main thread after Compile has completed.
  // TODO(v8:7700): Move this to a different class?
  static MaybeHandle<CodeT> GenerateCode(
      MaglevCompilationUnit* toplevel_compilation_unit);

 private:
  explicit MaglevCompiler(LocalIsolate* local_isolate,
                          MaglevCompilationUnit* toplevel_compilation_unit)
      : local_isolate_(local_isolate),
        toplevel_compilation_unit_(toplevel_compilation_unit) {}

  void Compile();

  compiler::JSHeapBroker* broker() const {
    return toplevel_compilation_unit_->broker();
  }
  Zone* zone() { return toplevel_compilation_unit_->zone(); }
  LocalIsolate* local_isolate() { return local_isolate_; }

  LocalIsolate* const local_isolate_;
  MaglevCompilationUnit* const toplevel_compilation_unit_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_COMPILER_H_
