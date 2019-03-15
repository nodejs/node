// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_SERVER_DATA_H_
#define V8_TORQUE_SERVER_DATA_H_

#include <map>
#include <vector>

#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/torque/source-positions.h"

namespace v8 {
namespace internal {
namespace torque {

// The definition of the token in the first element, can be found at the second.
using DefinitionMapping = std::pair<SourcePosition, SourcePosition>;
// TODO(szuend): Support overlapping source positions when we start adding them.
using Definitions = std::vector<DefinitionMapping>;
using DefinitionsMap = std::map<SourceId, Definitions>;

// This contextual class holds all the necessary data to answer incoming
// LSP requests. It is reset for each compilation step and all information
// is calculated eagerly during compilation.
class LanguageServerData : public ContextualClass<LanguageServerData> {
 public:
  LanguageServerData() = default;

  V8_EXPORT_PRIVATE static void AddDefinition(SourcePosition token,
                                              SourcePosition definition);

  V8_EXPORT_PRIVATE static base::Optional<SourcePosition> FindDefinition(
      SourceId source, LineAndColumn pos);

 private:
  DefinitionsMap definitions_map_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_SERVER_DATA_H_
