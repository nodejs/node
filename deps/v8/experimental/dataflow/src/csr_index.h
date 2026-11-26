// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSR_INDEX_H
#define CSR_INDEX_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "dfg.h"

namespace dfg {

// Zero-copy Read-Only Memory-Mapped Compressed Sparse Row (CSR/CSC) Index.
// Provides O(log N) node/spatial lookups and O(1) contiguous adjacency slices
// for incoming and outgoing edges without SQLite cursor overhead.
class CsrIndex {
 public:
  struct Header {
    char Magic[8];            // "DFGCSR03"
    uint64_t NumNodes;        // Count of unique Node::Ids in SortedIds/Nodes
    uint64_t NumEdges;        // Count of unique directed edges
    uint64_t NumLocEntries;   // Count of spatial (File, Line, Col) entries
    uint64_t NumFiles;        // Count of sorted unique file path offsets
    uint64_t StringBlobSize;  // Byte size of deduplicated StringBlob
    uint64_t NumNodeRecords;  // Exact count of valid node records (HasRecord == 1)
    uint64_t Reserved;
  };

  struct PackedNode {
    uint32_t NameOffset;
    uint32_t TypeOffset;
    uint32_t Line : 24;
    uint32_t Kind : 4;
    uint32_t HasRecord : 4;
    uint16_t FileIdx;
    uint16_t Column;
  };
  static_assert(sizeof(PackedNode) == 16, "PackedNode must be 16 bytes");

  struct PackedEdge {
    uint32_t PeerIdx;
    uint32_t Line : 24;
    uint32_t Kind : 8;
    uint16_t FileIdx;
    uint16_t Column;
  };
  static_assert(sizeof(PackedEdge) == 12, "PackedEdge must be 12 bytes");

  struct PackedLocEntry {
    uint16_t FileIdx;
    uint16_t Column;
    uint32_t Line;
    uint32_t NodeIdx;
  };
  static_assert(sizeof(PackedLocEntry) == 12,
                "PackedLocEntry must be 12 bytes");

  CsrIndex() = default;
  ~CsrIndex();

  CsrIndex(const CsrIndex&) = delete;
  CsrIndex& operator=(const CsrIndex&) = delete;

  bool Open(const std::string& Path);
  void Close();
  bool IsOpen() const { return MappedData != nullptr; }

  static bool Build(const std::string& OutputPath,
                    const std::vector<Node>& AllNodes,
                    const std::vector<Edge>& AllEdges);

  std::unique_ptr<Node> GetNodeById(Node::Id ID) const;
  std::vector<Edge> GetIncomingEdges(Node::Id ToId) const;
  std::vector<Edge> GetOutgoingEdges(Node::Id FromId) const;
  bool HasOutgoingImplicitEdge(Node::Id FromId) const;
  std::vector<Node> FindNodesAtLocation(const std::string& File, unsigned Line,
                                        unsigned Column) const;
  std::unique_ptr<Node> FindNodeByNameInFile(const std::string& File,
                                             unsigned Line,
                                             const std::string& Name) const;
  bool HasAnyEdges(Node::Id ID) const;

  uint64_t GetNumNodes() const { return Hdr ? Hdr->NumNodes : 0; }
  uint64_t GetNumNodeRecords() const { return Hdr ? Hdr->NumNodeRecords : 0; }
  uint64_t GetNumEdges() const { return Hdr ? Hdr->NumEdges : 0; }

 private:
  bool HasOutgoingImplicitEdgeByIdx(uint32_t FromIdx) const;
  bool FindNodeIdx(Node::Id ID, uint32_t& OutIdx) const;
  bool FindFileIdx(const std::string& File, uint16_t& OutIdx) const;
  const char* GetFileString(uint16_t FileIdx) const;
  const char* GetString(uint32_t Offset) const;
  Node ReconstructNode(uint32_t Idx) const;

  const uint8_t* MappedData = nullptr;
  size_t MappedSize = 0;
  int Fd = -1;

  const Header* Hdr = nullptr;
  const uint64_t* SortedIds = nullptr;
  const PackedNode* Nodes = nullptr;
  const uint32_t* ForwardOffsets = nullptr;
  const PackedEdge* ForwardEdges = nullptr;
  const uint32_t* BackwardOffsets = nullptr;
  const PackedEdge* BackwardEdges = nullptr;
  const uint32_t* FileOffsets = nullptr;
  const PackedLocEntry* LocEntries = nullptr;
  const char* StringBlob = nullptr;
};

}  // namespace dfg

#endif  // CSR_INDEX_H
