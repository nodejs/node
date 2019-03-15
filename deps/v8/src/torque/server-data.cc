// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/server-data.h"

namespace v8 {
namespace internal {
namespace torque {

DEFINE_CONTEXTUAL_VARIABLE(LanguageServerData)

void LanguageServerData::AddDefinition(SourcePosition token,
                                       SourcePosition definition) {
  Get().definitions_map_[token.source].emplace_back(token, definition);
}

base::Optional<SourcePosition> LanguageServerData::FindDefinition(
    SourceId source, LineAndColumn pos) {
  for (const DefinitionMapping& mapping : Get().definitions_map_.at(source)) {
    SourcePosition current = mapping.first;
    if (current.Contains(pos)) return mapping.second;
  }

  return base::nullopt;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
