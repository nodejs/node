// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TYPE_HINT_ANALYZER_H_
#define V8_COMPILER_TYPE_HINT_ANALYZER_H_

#include "src/handles.h"
#include "src/type-hints.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// The result of analyzing type hints.
class TypeHintAnalysis final : public ZoneObject {
 public:
  typedef ZoneMap<TypeFeedbackId, Handle<Code>> Infos;

  explicit TypeHintAnalysis(Infos const& infos, Zone* zone)
      : infos_(infos), zone_(zone) {}

  bool GetBinaryOperationHint(TypeFeedbackId id,
                              BinaryOperationHint* hint) const;
  bool GetCompareOperationHint(TypeFeedbackId id,
                               CompareOperationHint* hint) const;
  bool GetToBooleanHints(TypeFeedbackId id, ToBooleanHints* hints) const;

 private:
  Zone* zone() const { return zone_; }

  Infos const infos_;
  Zone* zone_;
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
