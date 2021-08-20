// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_VISUALIZER_H_
#define V8_COMPILER_GRAPH_VISUALIZER_H_

#include <stdio.h>

#include <fstream>
#include <iosfwd>
#include <memory>

#include "src/common/globals.h"
#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class OptimizedCompilationInfo;
class SharedFunctionInfo;
class SourcePosition;
namespace compiler {

class Graph;
class LiveRange;
class TopLevelLiveRange;
class Instruction;
class InstructionBlock;
class InstructionOperand;
class InstructionSequence;
class NodeOrigin;
class NodeOriginTable;
class RegisterAllocationData;
class Schedule;
class SourcePositionTable;

struct TurboJsonFile : public std::ofstream {
  TurboJsonFile(OptimizedCompilationInfo* info, std::ios_base::openmode mode);
  ~TurboJsonFile() override;
};

struct TurboCfgFile : public std::ofstream {
  explicit TurboCfgFile(Isolate* isolate = nullptr);
  ~TurboCfgFile() override;
};

struct SourcePositionAsJSON {
  explicit SourcePositionAsJSON(const SourcePosition& sp) : sp(sp) {}
  const SourcePosition& sp;
};

V8_INLINE V8_EXPORT_PRIVATE SourcePositionAsJSON
AsJSON(const SourcePosition& sp) {
  return SourcePositionAsJSON(sp);
}

struct NodeOriginAsJSON {
  explicit NodeOriginAsJSON(const NodeOrigin& no) : no(no) {}
  const NodeOrigin& no;
};

V8_INLINE V8_EXPORT_PRIVATE NodeOriginAsJSON AsJSON(const NodeOrigin& no) {
  return NodeOriginAsJSON(no);
}

std::ostream& operator<<(std::ostream& out, const SourcePositionAsJSON& pos);

// Small helper that deduplicates SharedFunctionInfos.
class V8_EXPORT_PRIVATE SourceIdAssigner {
 public:
  explicit SourceIdAssigner(size_t size) {
    printed_.reserve(size);
    source_ids_.reserve(size);
  }
  int GetIdFor(Handle<SharedFunctionInfo> shared);
  int GetIdAt(size_t pos) const { return source_ids_[pos]; }

 private:
  std::vector<Handle<SharedFunctionInfo>> printed_;
  std::vector<int> source_ids_;
};

void JsonPrintAllSourceWithPositions(std::ostream& os,
                                     OptimizedCompilationInfo* info,
                                     Isolate* isolate);

void JsonPrintFunctionSource(std::ostream& os, int source_id,
                             std::unique_ptr<char[]> function_name,
                             Handle<Script> script, Isolate* isolate,
                             Handle<SharedFunctionInfo> shared,
                             bool with_key = false);
std::unique_ptr<char[]> GetVisualizerLogFileName(OptimizedCompilationInfo* info,
                                                 const char* optional_base_dir,
                                                 const char* phase,
                                                 const char* suffix);

struct GraphAsJSON {
  GraphAsJSON(const Graph& g, SourcePositionTable* p, NodeOriginTable* o)
      : graph(g), positions(p), origins(o) {}
  const Graph& graph;
  const SourcePositionTable* positions;
  const NodeOriginTable* origins;
};

V8_INLINE V8_EXPORT_PRIVATE GraphAsJSON AsJSON(const Graph& g,
                                               SourcePositionTable* p,
                                               NodeOriginTable* o) {
  return GraphAsJSON(g, p, o);
}

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const GraphAsJSON& ad);

struct AsRPO {
  explicit AsRPO(const Graph& g) : graph(g) {}
  const Graph& graph;
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, const AsRPO& ad);

struct AsC1VCompilation {
  explicit AsC1VCompilation(const OptimizedCompilationInfo* info)
      : info_(info) {}
  const OptimizedCompilationInfo* info_;
};

struct AsScheduledGraph {
  explicit AsScheduledGraph(const Schedule* schedule) : schedule(schedule) {}
  const Schedule* schedule;
};

std::ostream& operator<<(std::ostream& os, const AsScheduledGraph& scheduled);
struct AsC1V {
  AsC1V(const char* phase, const Schedule* schedule,
        const SourcePositionTable* positions = nullptr,
        const InstructionSequence* instructions = nullptr)
      : schedule_(schedule),
        instructions_(instructions),
        positions_(positions),
        phase_(phase) {}
  const Schedule* schedule_;
  const InstructionSequence* instructions_;
  const SourcePositionTable* positions_;
  const char* phase_;
};

struct AsC1VRegisterAllocationData {
  explicit AsC1VRegisterAllocationData(
      const char* phase, const RegisterAllocationData* data = nullptr)
      : phase_(phase), data_(data) {}
  const char* phase_;
  const RegisterAllocationData* data_;
};

std::ostream& operator<<(std::ostream& os, const AsC1VCompilation& ac);
std::ostream& operator<<(std::ostream& os, const AsC1V& ac);
std::ostream& operator<<(std::ostream& os,
                         const AsC1VRegisterAllocationData& ac);

struct LiveRangeAsJSON {
  const LiveRange& range_;
  const InstructionSequence& code_;
};

std::ostream& operator<<(std::ostream& os,
                         const LiveRangeAsJSON& live_range_json);

struct TopLevelLiveRangeAsJSON {
  const TopLevelLiveRange& range_;
  const InstructionSequence& code_;
};

std::ostream& operator<<(
    std::ostream& os, const TopLevelLiveRangeAsJSON& top_level_live_range_json);

struct RegisterAllocationDataAsJSON {
  const RegisterAllocationData& data_;
  const InstructionSequence& code_;
};

std::ostream& operator<<(std::ostream& os,
                         const RegisterAllocationDataAsJSON& ac);

struct InstructionOperandAsJSON {
  const InstructionOperand* op_;
  const InstructionSequence* code_;
};

std::ostream& operator<<(std::ostream& os, const InstructionOperandAsJSON& o);

struct InstructionAsJSON {
  int index_;
  const Instruction* instr_;
  const InstructionSequence* code_;
};
std::ostream& operator<<(std::ostream& os, const InstructionAsJSON& i);

struct InstructionBlockAsJSON {
  const InstructionBlock* block_;
  const InstructionSequence* code_;
};

std::ostream& operator<<(std::ostream& os, const InstructionBlockAsJSON& b);

struct InstructionSequenceAsJSON {
  const InstructionSequence* sequence_;
};
std::ostream& operator<<(std::ostream& os, const InstructionSequenceAsJSON& s);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_VISUALIZER_H_
