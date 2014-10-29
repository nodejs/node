// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_VISUALIZER_H_
#define V8_COMPILER_GRAPH_VISUALIZER_H_

#include "src/v8.h"

namespace v8 {
namespace internal {

class OStream;

namespace compiler {

class Graph;

struct AsDOT {
  explicit AsDOT(const Graph& g) : graph(g) {}
  const Graph& graph;
};

OStream& operator<<(OStream& os, const AsDOT& ad);

struct AsJSON {
  explicit AsJSON(const Graph& g) : graph(g) {}
  const Graph& graph;
};

OStream& operator<<(OStream& os, const AsJSON& ad);
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_GRAPH_VISUALIZER_H_
