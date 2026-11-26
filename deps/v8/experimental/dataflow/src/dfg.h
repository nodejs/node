// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DFG_H
#define DFG_H

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace dfg {

struct ValueRange {
  bool HasValue = false;
  int64_t Min = 0;
  int64_t Max = 0;

  static ValueRange Unknown() { return {false, 0, 0}; }
  static ValueRange Exact(int64_t V) { return {true, V, V}; }
  static ValueRange Interval(int64_t Lo, int64_t Hi) {
    if (Lo > Hi) std::swap(Lo, Hi);
    return {true, Lo, Hi};
  }
  bool IsConstant() const { return HasValue && Min == Max; }
  std::string ToDisplayString() const {
    if (!HasValue) return "";
    auto FmtBound = [](int64_t V) -> std::string {
      if (V == std::numeric_limits<int64_t>::min()) return "-2^63";
      if (V == std::numeric_limits<int64_t>::max()) return "2^63-1";
      if (V == -2147483648LL) return "-2^31";
      if (V == 2147483647LL) return "2^31-1";
      if (V == 4294967295LL) return "2^32-1";
      return std::to_string(V);
    };
    if (Min == Max) return "[" + FmtBound(Min) + "]";
    return "[" + FmtBound(Min) + ", " + FmtBound(Max) + "]";
  }
};

enum class HeapTaint : uint8_t {
  None = 0,
  TrustedSpace = 1,
  Embedder = 2,
  InSandbox = 3,
};

inline HeapTaint CombineHeapTaint(HeapTaint A, HeapTaint B) {
  return static_cast<HeapTaint>(
      std::max(static_cast<uint8_t>(A), static_cast<uint8_t>(B)));
}

inline const char* HeapTaintToString(HeapTaint T) {
  switch (T) {
    case HeapTaint::TrustedSpace:
      return "trusted";
    case HeapTaint::Embedder:
      return "embedder";
    case HeapTaint::InSandbox:
      return "sandbox";
    default:
      return "";
  }
}

inline const char* HeapTaintTypeSuffix(HeapTaint T) {
  switch (T) {
    case HeapTaint::TrustedSpace:
      return " [heap:trusted]";
    case HeapTaint::Embedder:
      return " [heap:embedder]";
    case HeapTaint::InSandbox:
      return " [heap:sandbox]";
    default:
      return "";
  }
}

inline uint64_t HashFnv1a64(std::string_view S) {
  uint64_t Hash = 0xcbf29ce484222325ULL;
  for (char C : S) {
    // Cast to unsigned char first to prevent sign extension!
    Hash ^= static_cast<uint64_t>(static_cast<unsigned char>(C));
    Hash *= 0x100000001B3ULL;
  }
  return Hash;
}

enum class NodeKind {
  Unknown,
  Variable,    // Local variables
  Parameter,   // Function parameters
  Field,       // Member variables
  Literal,     // Constants (int, string, etc.)
  Function,    // Function declarations (sinks/sources)
  Expression,  // Temporary expressions
};

inline const char* NodeKindToString(NodeKind K) {
  switch (K) {
    case NodeKind::Variable:
      return "Variable";
    case NodeKind::Parameter:
      return "Parameter";
    case NodeKind::Field:
      return "Field";
    case NodeKind::Literal:
      return "Literal";
    case NodeKind::Function:
      return "Function";
    case NodeKind::Expression:
      return "Expression";
    default:
      return "Unknown";
  }
}

struct SourceLocation {
  std::string File;
  unsigned Line = 0;
  unsigned Column = 0;
  std::string MacroSpelling; // Capture spelling location of macro expansions to prevent collisions

  bool IsValid() const { return !File.empty(); }
};

struct Node {
  using Id = uint64_t;

  Id ID;
  NodeKind Kind = NodeKind::Unknown;
  std::string Name;
  std::string Type;
  SourceLocation Loc;
};

enum class EdgeKind {
  Unknown,
  Assignment,      // x = y
  Initialization,  // int x = y
  ArgPass,         // call(x) -> param y
  Return,          // return x -> call site result
  MemberAccess,    // x.y
  Implicit,        // Implicit conversions or flows
};

inline const char* EdgeKindToString(EdgeKind K) {
  switch (K) {
    case EdgeKind::Assignment:
      return "Assignment";
    case EdgeKind::Initialization:
      return "Initialization";
    case EdgeKind::ArgPass:
      return "ArgPass";
    case EdgeKind::Return:
      return "Return";
    case EdgeKind::MemberAccess:
      return "MemberAccess";
    case EdgeKind::Implicit:
      return "Implicit";
    default:
      return "Unknown";
  }
}

struct Edge {
  Node::Id From;
  Node::Id To;
  EdgeKind Kind = EdgeKind::Unknown;
  SourceLocation Loc;
};

struct DfgGraph {
  std::vector<Node> Nodes;
  std::vector<Edge> Edges;
  std::string FilePath;
  std::string FileHash;
  void Clear() {
    Nodes.clear();
    Edges.clear();
    FilePath.clear();
    FileHash.clear();
  }
};

#ifndef _WIN32
#include <unistd.h>
#endif

inline std::string GetDefaultCacheDir() {
#ifdef _WIN32
  return "C:\\Windows\\Temp\\v8-dataflow-cache";
#else
  return "/tmp/v8-dataflow-cache-" + std::to_string(getuid());
#endif
}

}  // namespace dfg

#endif  // DFG_H
