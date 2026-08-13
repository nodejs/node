// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_SERIALIZER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_SERIALIZER_H_

#include <fstream>
#include <ostream>

#include "src/handles/handles.h"
#include "src/maglev/maglev-phase.h"

namespace v8 {
namespace internal {
class Isolate;
class Code;
class Script;
class SharedFunctionInfo;

namespace maglev {

class Graph;
class MaglevCompilationInfo;

struct MaglevGraphAsJSON {
  Graph* graph;
};

inline MaglevGraphAsJSON AsJSON(Graph* graph) {
  return MaglevGraphAsJSON{graph};
}

std::ostream& operator<<(std::ostream& os, const MaglevGraphAsJSON& ad);

void JsonPrintFunctionSource(std::ostream& os, int source_id,
                             const std::string& function_name,
                             DirectHandle<Script> script, Isolate* isolate,
                             DirectHandle<SharedFunctionInfo> shared,
                             bool with_key = false);

class MaglevJsonFile : public std::ofstream {
 public:
  MaglevJsonFile(MaglevCompilationInfo* info, std::ios_base::openmode mode);
  ~MaglevJsonFile() override;
};

void PrintMaglevGraphAsJSON(MaglevCompilationInfo* info, Graph* graph,
                            MaglevPhase phase);

void FinalizeMaglevLogging(Isolate* isolate, MaglevCompilationInfo* info,
                           Graph* graph, Handle<Code> code);

}  // namespace maglev

}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_SERIALIZER_H_
