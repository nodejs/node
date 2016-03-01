// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TYPE_HINT_ANALYZER_H_
#define V8_COMPILER_TYPE_HINT_ANALYZER_H_

#include "src/compiler/type-hints.h"
#include "src/handles.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// The result of analyzing type hints.
class TypeHintAnalysis final : public ZoneObject {
 public:
  typedef ZoneMap<TypeFeedbackId, Handle<Code>> Infos;

  explicit TypeHintAnalysis(Infos const& infos) : infos_(infos) {}

  bool GetBinaryOperationHints(TypeFeedbackId id,
                               BinaryOperationHints* hints) const;
  bool GetToBooleanHints(TypeFeedbackId id, ToBooleanHints* hints) const;

 private:
  Infos const infos_;
};


// The class that performs type hint analysis on the fullcodegen code object.
class TypeHintAnalyzer final {
 public:
  explicit TypeHintAnalyzer(Zone* zone) : zone_(zone) {}

  TypeHintAnalysis* Analyze(Handle<Code> code);

 private:
  Zone* zone() const { return zone_; }

  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(TypeHintAnalyzer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TYPE_HINT_ANALYZER_H_
