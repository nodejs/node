// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_INFER_REPRESENTATION_H_
#define V8_HYDROGEN_INFER_REPRESENTATION_H_

#include "src/hydrogen.h"

namespace v8 {
namespace internal {


class HInferRepresentationPhase : public HPhase {
 public:
  explicit HInferRepresentationPhase(HGraph* graph)
      : HPhase("H_Infer representations", graph),
        worklist_(8, zone()),
        in_worklist_(graph->GetMaximumValueID(), zone()) { }

  void Run();
  void AddToWorklist(HValue* current);

 private:
  ZoneList<HValue*> worklist_;
  BitVector in_worklist_;

  DISALLOW_COPY_AND_ASSIGN(HInferRepresentationPhase);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_INFER_REPRESENTATION_H_
