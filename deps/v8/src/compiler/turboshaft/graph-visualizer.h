// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_GRAPH_VISUALIZER_H_
#define V8_COMPILER_TURBOSHAFT_GRAPH_VISUALIZER_H_

#include "src/compiler/turboshaft/graph.h"

#include "src/common/globals.h"
#include "src/handles/handles.h"

namespace v8::internal::compiler::turboshaft {

struct TurboshaftGraphAsJSON {
  TurboshaftGraphAsJSON(const Graph& tg, Zone* z): turboshaft_graph(tg),
                                                    temp_zone(z){}
  const Graph& turboshaft_graph;
  Zone* temp_zone;
};

V8_INLINE V8_EXPORT_PRIVATE TurboshaftGraphAsJSON AsJSON(const Graph& tg,
                                                         Zone* z) {
  return TurboshaftGraphAsJSON(tg, z);
}

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const TurboshaftGraphAsJSON& ad);

class JSONTurboshaftGraphWriter {
 public:
  JSONTurboshaftGraphWriter(std::ostream& os, const Graph& turboshaft_graph,
                            Zone* zone);

  JSONTurboshaftGraphWriter(const JSONTurboshaftGraphWriter&) = delete;
  JSONTurboshaftGraphWriter& operator=(const JSONTurboshaftGraphWriter&) = delete;

  void Print();

 protected:
  void PrintNodes();
  void PrintEdges();
  void PrintBlocks();

 protected:
  std::ostream& os_;
  Zone* zone_;
  const Graph& turboshaft_graph_;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_GRAPH_VISUALIZER_H_
