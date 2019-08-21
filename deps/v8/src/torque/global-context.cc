// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/global-context.h"

namespace v8 {
namespace internal {
namespace torque {

GlobalContext::GlobalContext(Ast ast)
    : collect_language_server_data_(false),
      force_assert_statements_(false),
      ast_(std::move(ast)) {
  CurrentScope::Scope current_scope(nullptr);
  CurrentSourcePosition::Scope current_source_position(
      SourcePosition{CurrentSourceFile::Get(), {-1, -1}, {-1, -1}});
  default_namespace_ =
      RegisterDeclarable(base::make_unique<Namespace>(kBaseNamespaceName));
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
