// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "csr_index.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>

namespace dfg {

static constexpr char kCsrMagic[8] = {'D', 'F', 'G', 'C', 'S', 'R', '0', '3'};

static size_t AlignUp8(size_t Offset) { return (Offset + 7u) & ~size_t(7u); }

CsrIndex::~CsrIndex() { Close(); }

void CsrIndex::Close() {
  if (MappedData && MappedData != MAP_FAILED) {
    munmap(const_cast<uint8_t*>(MappedData), MappedSize);
  }
  if (Fd >= 0) {
    close(Fd);
    Fd = -1;
  }
  MappedData = nullptr;
  MappedSize = 0;
  Hdr = nullptr;
  SortedIds = nullptr;
  Nodes = nullptr;
  ForwardOffsets = nullptr;
  ForwardEdges = nullptr;
  BackwardOffsets = nullptr;
  BackwardEdges = nullptr;
  FileOffsets = nullptr;
  LocEntries = nullptr;
  StringBlob = nullptr;
}

bool CsrIndex::Open(const std::string& Path) {
  Close();

  Fd = open(Path.c_str(), O_RDONLY);
  if (Fd < 0) {
    return false;
  }

  struct stat St;
  if (fstat(Fd, &St) != 0 ||
      static_cast<size_t>(St.st_size) < sizeof(Header)) {
    close(Fd);
    Fd = -1;
    return false;
  }

  MappedSize = static_cast<size_t>(St.st_size);
  void* Ptr = mmap(nullptr, MappedSize, PROT_READ, MAP_SHARED, Fd, 0);
  if (Ptr == MAP_FAILED) {
    close(Fd);
    Fd = -1;
    MappedSize = 0;
    return false;
  }

  MappedData = static_cast<const uint8_t*>(Ptr);
  Hdr = reinterpret_cast<const Header*>(MappedData);
  if (std::memcmp(Hdr->Magic, kCsrMagic, sizeof(kCsrMagic)) != 0) {
    Close();
    return false;
  }

  size_t Offset = AlignUp8(sizeof(Header));
  uint64_t N = Hdr->NumNodes;
  uint64_t M = Hdr->NumEdges;
  uint64_t L = Hdr->NumLocEntries;
  uint64_t F = Hdr->NumFiles;
  uint64_t S = Hdr->StringBlobSize;

  auto AdvanceSection = [&](size_t Bytes) -> const void* {
    if (Offset + Bytes > MappedSize) return nullptr;
    const void* SectionPtr = MappedData + Offset;
    Offset = AlignUp8(Offset + Bytes);
    return SectionPtr;
  };

  SortedIds = static_cast<const uint64_t*>(AdvanceSection(N * sizeof(uint64_t)));
  Nodes = static_cast<const PackedNode*>(AdvanceSection(N * sizeof(PackedNode)));
  ForwardOffsets =
      static_cast<const uint32_t*>(AdvanceSection((N + 1) * sizeof(uint32_t)));
  ForwardEdges =
      static_cast<const PackedEdge*>(AdvanceSection(M * sizeof(PackedEdge)));
  BackwardOffsets =
      static_cast<const uint32_t*>(AdvanceSection((N + 1) * sizeof(uint32_t)));
  BackwardEdges =
      static_cast<const PackedEdge*>(AdvanceSection(M * sizeof(PackedEdge)));
  FileOffsets =
      static_cast<const uint32_t*>(AdvanceSection(F * sizeof(uint32_t)));
  LocEntries = static_cast<const PackedLocEntry*>(
      AdvanceSection(L * sizeof(PackedLocEntry)));

  if (Offset + S > MappedSize || !SortedIds || !Nodes || !ForwardOffsets ||
      !ForwardEdges || !BackwardOffsets || !BackwardEdges || !FileOffsets ||
      !LocEntries) {
    Close();
    return false;
  }
  StringBlob = reinterpret_cast<const char*>(MappedData + Offset);
  return true;
}

const char* CsrIndex::GetString(uint32_t Offset) const {
  if (!StringBlob || !Hdr || Offset >= Hdr->StringBlobSize) {
    return "";
  }
  return StringBlob + Offset;
}

const char* CsrIndex::GetFileString(uint16_t FileIdx) const {
  if (!FileOffsets || !Hdr || FileIdx >= Hdr->NumFiles) {
    return "";
  }
  return GetString(FileOffsets[FileIdx]);
}

bool CsrIndex::FindNodeIdx(Node::Id ID, uint32_t& OutIdx) const {
  if (!SortedIds || !Hdr || Hdr->NumNodes == 0) return false;
  const uint64_t* Begin = SortedIds;
  const uint64_t* End = SortedIds + Hdr->NumNodes;
  const uint64_t* It = std::lower_bound(Begin, End, ID);
  if (It == End || *It != ID) return false;
  OutIdx = static_cast<uint32_t>(It - Begin);
  return true;
}

bool CsrIndex::FindFileIdx(const std::string& File, uint16_t& OutIdx) const {
  if (!FileOffsets || !Hdr || Hdr->NumFiles == 0) return false;
  const uint32_t* Begin = FileOffsets;
  const uint32_t* End = FileOffsets + Hdr->NumFiles;
  const uint32_t* It = std::lower_bound(
      Begin, End, File, [this](uint32_t Off, const std::string& Target) {
        return std::strcmp(GetString(Off), Target.c_str()) < 0;
      });
  if (It == End || std::strcmp(GetString(*It), File.c_str()) != 0) {
    return false;
  }
  OutIdx = static_cast<uint16_t>(It - Begin);
  return true;
}

Node CsrIndex::ReconstructNode(uint32_t Idx) const {
  const PackedNode& PN = Nodes[Idx];
  Node N;
  N.ID = SortedIds[Idx];
  N.Kind = static_cast<NodeKind>(PN.Kind);
  N.Name = GetString(PN.NameOffset);
  N.Type = GetString(PN.TypeOffset);
  N.Loc.File = GetFileString(PN.FileIdx);
  N.Loc.Line = PN.Line;
  N.Loc.Column = PN.Column;
  return N;
}

std::unique_ptr<Node> CsrIndex::GetNodeById(Node::Id ID) const {
  uint32_t Idx = 0;
  if (!FindNodeIdx(ID, Idx)) return nullptr;
  if (!Nodes[Idx].HasRecord) return nullptr;
  return std::make_unique<Node>(ReconstructNode(Idx));
}

std::vector<Edge> CsrIndex::GetIncomingEdges(Node::Id ToId) const {
  std::vector<Edge> Result;
  uint32_t ToIdx = 0;
  if (!FindNodeIdx(ToId, ToIdx)) return Result;

  uint32_t Start = BackwardOffsets[ToIdx];
  uint32_t End = BackwardOffsets[ToIdx + 1];
  Result.reserve(End - Start);
  for (uint32_t i = Start; i < End; ++i) {
    const PackedEdge& PE = BackwardEdges[i];
    Edge E;
    E.From = SortedIds[PE.PeerIdx];
    E.To = ToId;
    E.Kind = static_cast<EdgeKind>(PE.Kind);
    E.Loc.File = GetFileString(PE.FileIdx);
    E.Loc.Line = PE.Line;
    E.Loc.Column = PE.Column;
    Result.push_back(std::move(E));
  }

  return Result;
}

std::vector<Edge> CsrIndex::GetOutgoingEdges(Node::Id FromId) const {
  std::vector<Edge> Result;
  uint32_t FromIdx = 0;
  if (!FindNodeIdx(FromId, FromIdx)) return Result;

  uint32_t Start = ForwardOffsets[FromIdx];
  uint32_t End = ForwardOffsets[FromIdx + 1];
  Result.reserve(End - Start);
  for (uint32_t i = Start; i < End; ++i) {
    const PackedEdge& PE = ForwardEdges[i];
    Edge E;
    E.From = FromId;
    E.To = SortedIds[PE.PeerIdx];
    E.Kind = static_cast<EdgeKind>(PE.Kind);
    E.Loc.File = GetFileString(PE.FileIdx);
    E.Loc.Line = PE.Line;
    E.Loc.Column = PE.Column;
    Result.push_back(std::move(E));
  }
  return Result;
}

bool CsrIndex::HasOutgoingImplicitEdgeByIdx(uint32_t FromIdx) const {
  const PackedNode& PN = Nodes[FromIdx];
  uint32_t Start = ForwardOffsets[FromIdx];
  uint32_t End = ForwardOffsets[FromIdx + 1];
  bool HasCrossLocImplicit = false;
  for (uint32_t i = Start; i < End; ++i) {
    if (static_cast<EdgeKind>(ForwardEdges[i].Kind) == EdgeKind::Implicit) {
      uint32_t PeerIdx = ForwardEdges[i].PeerIdx;
      const PackedNode& PeerPN = Nodes[PeerIdx];
      if (PeerPN.FileIdx == PN.FileIdx && PeerPN.Line == PN.Line &&
          PeerPN.Column == PN.Column) {
        return false;
      }
      HasCrossLocImplicit = true;
    }
  }
  return HasCrossLocImplicit;
}

bool CsrIndex::HasOutgoingImplicitEdge(Node::Id FromId) const {
  uint32_t FromIdx = 0;
  if (!FindNodeIdx(FromId, FromIdx)) return false;
  return HasOutgoingImplicitEdgeByIdx(FromIdx);
}

bool CsrIndex::HasAnyEdges(Node::Id ID) const {
  uint32_t Idx = 0;
  if (!FindNodeIdx(ID, Idx)) return false;
  return (ForwardOffsets[Idx + 1] > ForwardOffsets[Idx]) ||
         (BackwardOffsets[Idx + 1] > BackwardOffsets[Idx]);
}

std::vector<Node> CsrIndex::FindNodesAtLocation(const std::string& File,
                                                unsigned Line,
                                                unsigned Column) const {
  std::vector<Node> Result;
  uint16_t TargetFileIdx = 0;
  if (!FindFileIdx(File, TargetFileIdx)) return Result;

  const PackedLocEntry* Begin = LocEntries;
  const PackedLocEntry* End = LocEntries + Hdr->NumLocEntries;
  uint16_t ClampedCol = static_cast<uint16_t>(std::min<unsigned>(Column, 65535u));
  PackedLocEntry Probe{TargetFileIdx, ClampedCol, Line, 0xFFFFFFFFu};

  const PackedLocEntry* It = std::upper_bound(
      Begin, End, Probe,
      [](const PackedLocEntry& A, const PackedLocEntry& B) {
        if (A.FileIdx != B.FileIdx) return A.FileIdx < B.FileIdx;
        if (A.Line != B.Line) return A.Line < B.Line;
        return A.Column < B.Column;
      });

  if (It != Begin) {
    --It;
    if (It->FileIdx == TargetFileIdx && It->Line == Line &&
        It->Column <= ClampedCol) {
      uint16_t MatchedCol = It->Column;
      while (true) {
        Result.push_back(ReconstructNode(It->NodeIdx));
        if (It == Begin) break;
        --It;
        if (It->FileIdx != TargetFileIdx || It->Line != Line ||
            It->Column != MatchedCol) {
          break;
        }
      }
    }
  }
  return Result;
}

std::unique_ptr<Node> CsrIndex::FindNodeByNameInFile(
    const std::string& File, unsigned Line, const std::string& Name) const {
  if (Name.empty() || !Hdr) return nullptr;
  uint16_t TargetFileIdx = 0;
  if (!FindFileIdx(File, TargetFileIdx)) return nullptr;

  const PackedLocEntry* Begin = LocEntries;
  const PackedLocEntry* End = LocEntries + Hdr->NumLocEntries;
  PackedLocEntry FileStartProbe{TargetFileIdx, 0, 0, 0};
  PackedLocEntry FileEndProbe{static_cast<uint16_t>(TargetFileIdx + 1), 0, 0,
                              0};
  auto Cmp = [](const PackedLocEntry& A, const PackedLocEntry& B) {
    if (A.FileIdx != B.FileIdx) return A.FileIdx < B.FileIdx;
    if (A.Line != B.Line) return A.Line < B.Line;
    return A.Column < B.Column;
  };
  const PackedLocEntry* FileBegin =
      std::lower_bound(Begin, End, FileStartProbe, Cmp);
  const PackedLocEntry* FileEnd =
      std::lower_bound(FileBegin, End, FileEndProbe, Cmp);

  std::string CallName = Name + "()";
  uint32_t BestBeforeIdx = 0xFFFFFFFFu;
  uint32_t BestBeforeLine = 0;
  int BestBeforeEdgeScore = -1;
  uint32_t BestAfterIdx = 0xFFFFFFFFu;
  uint32_t BestAfterLineDiff = 0xFFFFFFFFu;
  int BestAfterEdgeScore = -1;

  auto HasEdgeNearLine = [&](uint32_t Idx, unsigned TargetLine) {
    for (uint32_t e = ForwardOffsets[Idx]; e < ForwardOffsets[Idx + 1]; ++e) {
      uint32_t ELine = ForwardEdges[e].Line;
      uint32_t Diff =
          (ELine > TargetLine) ? (ELine - TargetLine) : (TargetLine - ELine);
      if (Diff <= 25) return true;
    }
    for (uint32_t e = BackwardOffsets[Idx]; e < BackwardOffsets[Idx + 1]; ++e) {
      uint32_t ELine = BackwardEdges[e].Line;
      uint32_t Diff =
          (ELine > TargetLine) ? (ELine - TargetLine) : (TargetLine - ELine);
      if (Diff <= 25) return true;
    }
    return false;
  };

  for (const PackedLocEntry* It = FileBegin; It != FileEnd; ++It) {
    uint32_t Idx = It->NodeIdx;
    const PackedNode& PN = Nodes[Idx];
    if (!PN.HasRecord) continue;
    NodeKind Kind = static_cast<NodeKind>(PN.Kind);
    if (Kind == NodeKind::Parameter && HasOutgoingImplicitEdgeByIdx(Idx)) {
      continue;
    }
    bool HasOut = (ForwardOffsets[Idx + 1] > ForwardOffsets[Idx]);
    bool HasIn = (BackwardOffsets[Idx + 1] > BackwardOffsets[Idx]);
    if (!HasOut && !HasIn) continue;
    const char* NStr = GetString(PN.NameOffset);
    if (Name != NStr) continue;

    int EdgeScore = (HasOut ? 1 : 0) + (HasIn ? 1 : 0);
    if (It->Line <= Line) {
      if ((Line - It->Line) > 250 && !HasEdgeNearLine(Idx, Line)) {
        continue;
      }
      if (BestBeforeIdx == 0xFFFFFFFFu || It->Line > BestBeforeLine ||
          (It->Line == BestBeforeLine && EdgeScore > BestBeforeEdgeScore)) {
        BestBeforeIdx = Idx;
        BestBeforeLine = It->Line;
        BestBeforeEdgeScore = EdgeScore;
      }
    } else {
      uint32_t Diff = It->Line - Line;
      if (Diff < BestAfterLineDiff ||
          (Diff == BestAfterLineDiff && EdgeScore > BestAfterEdgeScore)) {
        BestAfterLineDiff = Diff;
        BestAfterIdx = Idx;
        BestAfterEdgeScore = EdgeScore;
      }
    }
  }
  if (BestBeforeIdx != 0xFFFFFFFFu) {
    return std::make_unique<Node>(ReconstructNode(BestBeforeIdx));
  }
  if (BestAfterIdx != 0xFFFFFFFFu && BestAfterLineDiff <= 25) {
    return std::make_unique<Node>(ReconstructNode(BestAfterIdx));
  }

  // Pass 2: Check 1-hop and 2-hop edge neighbors of nodes in this file (sorted
  // by proximity to Line) to resolve class Fields (e.g., LookupIterator::state_
  // declared in lookup.h and assigned/read in lookup.cc).
  uint32_t BestNbrIdx = 0xFFFFFFFFu;
  uint32_t BestNbrLineDiff = 0xFFFFFFFFu;
  auto CheckCandidate = [&](uint32_t CandIdx, uint32_t AnchorLine) {
    const PackedNode& CandPN = Nodes[CandIdx];
    if (!CandPN.HasRecord) return;
    const char* CandName = GetString(CandPN.NameOffset);
    if (Name != CandName && CallName != CandName) return;
    uint32_t Diff = (AnchorLine > Line) ? (AnchorLine - Line)
                                        : (Line - AnchorLine);
    if (Diff < BestNbrLineDiff) {
      BestNbrLineDiff = Diff;
      BestNbrIdx = CandIdx;
    }
  };

  for (const PackedLocEntry* It = FileBegin; It != FileEnd; ++It) {
    uint32_t Diff = (It->Line > Line) ? (It->Line - Line) : (Line - It->Line);
    if (Diff >= BestNbrLineDiff) continue;
    uint32_t Idx = It->NodeIdx;
    for (uint32_t e = ForwardOffsets[Idx]; e < ForwardOffsets[Idx + 1]; ++e) {
      uint32_t TgtIdx = ForwardEdges[e].PeerIdx;
      CheckCandidate(TgtIdx, It->Line);
      const char* TgtName = GetString(Nodes[TgtIdx].NameOffset);
      if (std::strstr(TgtName, "_return")) {
        for (uint32_t e2 = ForwardOffsets[TgtIdx];
             e2 < ForwardOffsets[TgtIdx + 1]; ++e2) {
          CheckCandidate(ForwardEdges[e2].PeerIdx, It->Line);
        }
      }
    }
    for (uint32_t e = BackwardOffsets[Idx]; e < BackwardOffsets[Idx + 1]; ++e) {
      uint32_t SrcIdx = BackwardEdges[e].PeerIdx;
      CheckCandidate(SrcIdx, It->Line);
    }
  }
  if (BestNbrIdx != 0xFFFFFFFFu) {
    return std::make_unique<Node>(ReconstructNode(BestNbrIdx));
  }
  return nullptr;
}

bool CsrIndex::Build(const std::string& OutputPath,
                     const std::vector<Node>& AllNodes,
                     const std::vector<Edge>& AllEdges) {
  // 1. Collect all unique Node::Ids (including edge-only endpoints)
  std::vector<uint64_t> SortedIdVec;
  SortedIdVec.reserve(AllNodes.size() + AllEdges.size());
  for (const auto& N : AllNodes) {
    SortedIdVec.push_back(N.ID);
  }
  for (const auto& E : AllEdges) {
    SortedIdVec.push_back(E.From);
    SortedIdVec.push_back(E.To);
  }
  std::sort(SortedIdVec.begin(), SortedIdVec.end());
  SortedIdVec.erase(std::unique(SortedIdVec.begin(), SortedIdVec.end()),
                    SortedIdVec.end());

  const uint32_t NumNodes = static_cast<uint32_t>(SortedIdVec.size());
  auto IdToIdx = [&SortedIdVec](Node::Id ID) -> uint32_t {
    auto It = std::lower_bound(SortedIdVec.begin(), SortedIdVec.end(), ID);
    return static_cast<uint32_t>(It - SortedIdVec.begin());
  };

  // 2. Collect and sort unique file paths first so FileOffsets are ordered
  std::vector<std::string> UniqueFiles;
  UniqueFiles.push_back("");  // Ensure index 0 is empty file path
  for (const auto& N : AllNodes) {
    if (!N.Loc.File.empty()) UniqueFiles.push_back(N.Loc.File);
  }
  for (const auto& E : AllEdges) {
    if (!E.Loc.File.empty()) UniqueFiles.push_back(E.Loc.File);
  }
  std::sort(UniqueFiles.begin(), UniqueFiles.end());
  UniqueFiles.erase(std::unique(UniqueFiles.begin(), UniqueFiles.end()),
                    UniqueFiles.end());

  std::string StringPool;
  StringPool.push_back('\0');  // Offset 0 is always the empty string ""
  std::unordered_map<std::string, uint32_t> StringToOffset;
  StringToOffset.emplace("", 0u);

  auto InternString = [&StringPool, &StringToOffset](const std::string& S) {
    auto It = StringToOffset.find(S);
    if (It != StringToOffset.end()) return It->second;
    uint32_t Off = static_cast<uint32_t>(StringPool.size());
    StringPool.append(S);
    StringPool.push_back('\0');
    StringToOffset.emplace(S, Off);
    return Off;
  };

  std::vector<uint32_t> FileOffsetsVec;
  FileOffsetsVec.reserve(UniqueFiles.size());
  std::unordered_map<std::string, uint16_t> FileToIdx;
  for (size_t i = 0; i < UniqueFiles.size(); ++i) {
    FileOffsetsVec.push_back(InternString(UniqueFiles[i]));
    FileToIdx.emplace(UniqueFiles[i], static_cast<uint16_t>(i));
  }

  auto GetInternedFileIdx = [&FileToIdx](const std::string& F) -> uint16_t {
    auto It = FileToIdx.find(F);
    return It != FileToIdx.end() ? It->second : 0;
  };

  // 3. Build PackedNodes array and Spatial Location Index
  std::vector<PackedNode> NodesVec(NumNodes, PackedNode{0, 0, 0, 0, 0, 0, 0});
  std::vector<PackedLocEntry> LocEntriesVec;
  LocEntriesVec.reserve(AllNodes.size());
  uint64_t ValidNodeRecordCount = 0;

  for (const auto& N : AllNodes) {
    uint32_t Idx = IdToIdx(N.ID);
    if (NodesVec[Idx].HasRecord) continue;
    PackedNode PN{};
    PN.NameOffset = InternString(N.Name);
    PN.TypeOffset = InternString(N.Type);
    PN.Line = std::min<unsigned>(N.Loc.Line, 0xFFFFFFu);
    PN.Kind = static_cast<uint32_t>(N.Kind) & 0xFu;
    PN.HasRecord = 1;
    PN.FileIdx = GetInternedFileIdx(N.Loc.File);
    PN.Column = static_cast<uint16_t>(std::min<unsigned>(N.Loc.Column, 65535u));
    NodesVec[Idx] = PN;
    ValidNodeRecordCount++;

    if (!N.Loc.File.empty()) {
      LocEntriesVec.push_back(
          PackedLocEntry{PN.FileIdx, PN.Column, N.Loc.Line, Idx});
    }
  }

  std::sort(LocEntriesVec.begin(), LocEntriesVec.end(),
            [](const PackedLocEntry& A, const PackedLocEntry& B) {
              if (A.FileIdx != B.FileIdx) return A.FileIdx < B.FileIdx;
              if (A.Line != B.Line) return A.Line < B.Line;
              if (A.Column != B.Column) return A.Column < B.Column;
              return A.NodeIdx < B.NodeIdx;
            });

  // 4. Build Forward CSR and Backward CSC adjacency tables
  const uint32_t NumEdges = static_cast<uint32_t>(AllEdges.size());
  std::vector<uint32_t> ForwardOffsetsVec(NumNodes + 1, 0);
  std::vector<uint32_t> BackwardOffsetsVec(NumNodes + 1, 0);

  struct ResolvedEdge {
    uint32_t FromIdx;
    uint32_t ToIdx;
    uint32_t Line;
    uint16_t FileIdx;
    uint16_t Column;
    uint8_t Kind;
  };
  std::vector<ResolvedEdge> ResolvedEdges;
  ResolvedEdges.reserve(NumEdges);

  for (const auto& E : AllEdges) {
    uint32_t U = IdToIdx(E.From);
    uint32_t V = IdToIdx(E.To);
    uint16_t FIdx = GetInternedFileIdx(E.Loc.File);
    uint16_t Col =
        static_cast<uint16_t>(std::min<unsigned>(E.Loc.Column, 65535u));
    uint8_t K = static_cast<uint8_t>(E.Kind);
    ResolvedEdges.push_back(
        {U, V, std::min<unsigned>(E.Loc.Line, 0xFFFFFFu), FIdx, Col, K});
    ForwardOffsetsVec[U + 1]++;
    BackwardOffsetsVec[V + 1]++;
  }

  for (uint32_t i = 0; i < NumNodes; ++i) {
    ForwardOffsetsVec[i + 1] += ForwardOffsetsVec[i];
    BackwardOffsetsVec[i + 1] += BackwardOffsetsVec[i];
  }

  std::vector<PackedEdge> ForwardEdgesVec(NumEdges);
  std::vector<PackedEdge> BackwardEdgesVec(NumEdges);
  std::vector<uint32_t> ForwardCursor = ForwardOffsetsVec;
  std::vector<uint32_t> BackwardCursor = BackwardOffsetsVec;

  for (const auto& RE : ResolvedEdges) {
    ForwardEdgesVec[ForwardCursor[RE.FromIdx]++] =
        PackedEdge{RE.ToIdx, RE.Line, RE.Kind, RE.FileIdx, RE.Column};
    BackwardEdgesVec[BackwardCursor[RE.ToIdx]++] =
        PackedEdge{RE.FromIdx, RE.Line, RE.Kind, RE.FileIdx, RE.Column};
  }

  // 5. Write atomically via temporary file + rename
  std::string TmpPath = OutputPath + ".tmp." + std::to_string(getpid());
  std::ofstream Out(TmpPath, std::ios::binary | std::ios::trunc);
  if (!Out.is_open()) return false;

  Header Hdr{};
  std::memcpy(Hdr.Magic, kCsrMagic, sizeof(kCsrMagic));
  Hdr.NumNodes = NumNodes;
  Hdr.NumEdges = NumEdges;
  Hdr.NumLocEntries = LocEntriesVec.size();
  Hdr.NumFiles = FileOffsetsVec.size();
  Hdr.StringBlobSize = StringPool.size();
  Hdr.NumNodeRecords = ValidNodeRecordCount;

  size_t Offset = 0;
  auto WriteSection = [&](const void* Data, size_t Bytes) {
    if (Bytes > 0) {
      Out.write(reinterpret_cast<const char*>(Data),
                static_cast<std::streamsize>(Bytes));
    }
    Offset += Bytes;
    size_t Aligned = AlignUp8(Offset);
    if (Aligned > Offset) {
      static const char kZeroPadding[8] = {0};
      Out.write(kZeroPadding, static_cast<std::streamsize>(Aligned - Offset));
      Offset = Aligned;
    }
  };

  WriteSection(&Hdr, sizeof(Header));
  WriteSection(SortedIdVec.data(), SortedIdVec.size() * sizeof(uint64_t));
  WriteSection(NodesVec.data(), NodesVec.size() * sizeof(PackedNode));
  WriteSection(ForwardOffsetsVec.data(),
               ForwardOffsetsVec.size() * sizeof(uint32_t));
  WriteSection(ForwardEdgesVec.data(),
               ForwardEdgesVec.size() * sizeof(PackedEdge));
  WriteSection(BackwardOffsetsVec.data(),
               BackwardOffsetsVec.size() * sizeof(uint32_t));
  WriteSection(BackwardEdgesVec.data(),
               BackwardEdgesVec.size() * sizeof(PackedEdge));
  WriteSection(FileOffsetsVec.data(),
               FileOffsetsVec.size() * sizeof(uint32_t));
  WriteSection(LocEntriesVec.data(),
               LocEntriesVec.size() * sizeof(PackedLocEntry));
  Out.write(StringPool.data(),
            static_cast<std::streamsize>(StringPool.size()));
  Out.close();

  if (!Out) {
    std::remove(TmpPath.c_str());
    return false;
  }
  if (std::rename(TmpPath.c_str(), OutputPath.c_str()) != 0) {
    std::remove(TmpPath.c_str());
    return false;
  }
  return true;
}

}  // namespace dfg
