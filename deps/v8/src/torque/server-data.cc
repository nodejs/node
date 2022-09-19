// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/server-data.h"

#include "src/torque/declarable.h"
#include "src/torque/implementation-visitor.h"

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
  if (!source.IsValid()) return base::nullopt;

  auto iter = Get().definitions_map_.find(source);
  if (iter == Get().definitions_map_.end()) return base::nullopt;

  for (const DefinitionMapping& mapping : iter->second) {
    SourcePosition current = mapping.first;
    if (current.Contains(pos)) return mapping.second;
  }

  return base::nullopt;
}

void LanguageServerData::PrepareAllDeclarableSymbols() {
  const std::vector<std::unique_ptr<Declarable>>& all_declarables =
      global_context_->declarables_;

  for (const auto& declarable : all_declarables) {
    // Class field accessors and implicit specializations are
    // auto-generated and should not show up.
    if (!declarable->IsUserDefined()) continue;

    SourceId source = declarable->Position().source;
    symbols_map_[source].push_back(declarable.get());
  }
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
