// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_VISUALIZER_H_
#define V8_COMPILER_GRAPH_VISUALIZER_H_

#include <iosfwd>

namespace v8 {
namespace internal {

class CompilationInfo;

namespace compiler {

class Graph;
class InstructionSequence;
class RegisterAllocator;
class Schedule;
class SourcePositionTable;


struct AsDOT {
  explicit AsDOT(const Graph& g) : graph(g) {}
  const Graph& graph;
};

std::ostream& operator<<(std::ostream& os, const AsDOT& ad);


struct AsJSON {
  explicit AsJSON(const Graph& g) : graph(g) {}
  const Graph& graph;
};

std::ostream& operator<<(std::ostream& os, const AsJSON& ad);

struct AsRPO {
  explicit AsRPO(const Graph& g) : graph(g) {}
  const Graph& graph;
};

std::ostream& operator<<(std::ostream& os, const AsRPO& ad);


struct AsC1VCompilation {
  explicit AsC1VCompilation(const CompilationInfo* info) : info_(info) {}
  const CompilationInfo* info_;
};


struct AsC1V {
  AsC1V(const char* phase, const Schedule* schedule,
        const SourcePositionTable* positions = NULL,
        const InstructionSequence* instructions = NULL)
      : schedule_(schedule),
        instructions_(instructions),
        positions_(positions),
        phase_(phase) {}
  const Schedule* schedule_;
  const InstructionSequence* instructions_;
  const SourcePositionTable* positions_;
  const char* phase_;
};

struct AsC1VAllocator {
  explicit AsC1VAllocator(const char* phase,
                          const RegisterAllocator* allocator = NULL)
      : phase_(phase), allocator_(allocator) {}
  const char* phase_;
  const RegisterAllocator* allocator_;
};

std::ostream& operator<<(std::ostream& os, const AsDOT& ad);
std::ostream& operator<<(std::ostream& os, const AsC1VCompilation& ac);
std::ostream& operator<<(std::ostream& os, const AsC1V& ac);
std::ostream& operator<<(std::ostream& os, const AsC1VAllocator& ac);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_VISUALIZER_H_
