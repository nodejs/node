// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "csr_index.h"
#include "dfg.h"

namespace dfg {

class Database {
 public:
  Database();
  ~Database();

  bool Open(const std::string& DbPath);
  void Close();
  bool BuildAndLoadCsrIndex();
  bool HasCsrIndex() const { return Csr.IsOpen(); }

  // Transaction management
  bool BeginTransaction();
  bool CommitTransaction();
  bool RollbackTransaction();

  // Hash management
  std::string GetFileHash(const std::string& File);
  bool UpdateFileHash(const std::string& File, const std::string& Hash);

  // Incremental data cleanup
  bool DeleteFileData(const std::string& File);
  bool DeleteFilesData(const std::vector<std::string>& Files);

  // Insert data
  bool InsertNode(const Node& N);
  bool InsertEdge(const Edge& E);

  // Query data
  const Node* GetNode(
      const Node::Id& Id);  // Returns cached pointer or null. Caller does NOT
                            // own memory, managed by DB cache.
  std::vector<Edge> GetIncomingEdges(const Node::Id& ToId);
  std::vector<Edge> GetOutgoingEdges(const Node::Id& FromId);
  bool HasOutgoingImplicitEdge(uint64_t NodeId);
  const Node* FindNodeByLocation(const std::string& File, unsigned Line,
                                 unsigned Col);
  std::vector<Node> FindNodesAtLocation(const std::string& File, unsigned Line,
                                        unsigned Col);

  // Status / stats
  bool GetStats(int& NodesCount, int& EdgesCount);
  bool DumpAll(std::vector<Node>& OutNodes, std::vector<Edge>& OutEdges);
  bool MergeFrom(const std::string& OtherDbPath, bool EnableTelemetry = false);

  // Telemetry getters
  void SetEnableTelemetry(bool Enable) { EnableTelemetry = Enable; }
  double GetCumulativeInsertNodeTime() const {
    return CumulativeInsertNodeTime;
  }
  double GetCumulativeInsertEdgeTime() const {
    return CumulativeInsertEdgeTime;
  }
  double GetCumulativeDeleteTime() const { return CumulativeDeleteTime; }
  void ResetTelemetry() {
    CumulativeInsertNodeTime = 0;
    CumulativeInsertEdgeTime = 0;
    CumulativeDeleteTime = 0;
  }

 private:
  sqlite3* Db = nullptr;
  std::string CurrentDbPath;
  CsrIndex Csr;
  bool EnableTelemetry = false;

  // Prepared statements
  sqlite3_stmt* StmtInsertFile = nullptr;
  sqlite3_stmt* StmtGetFileId = nullptr;
  sqlite3_stmt* StmtInsertType = nullptr;
  sqlite3_stmt* StmtGetTypeId = nullptr;
  sqlite3_stmt* StmtInsertNode = nullptr;
  sqlite3_stmt* StmtInsertEdge = nullptr;
  sqlite3_stmt* StmtGetNode = nullptr;
  sqlite3_stmt* StmtGetIncoming = nullptr;
  sqlite3_stmt* StmtGetOutgoing = nullptr;
  sqlite3_stmt* StmtHasOutgoingImplicit = nullptr;
  sqlite3_stmt* StmtFindNode = nullptr;
  sqlite3_stmt* StmtGetHash = nullptr;
  sqlite3_stmt* StmtUpdateHash = nullptr;
  sqlite3_stmt* StmtDeleteHash = nullptr;
  sqlite3_stmt* StmtDeleteNodes = nullptr;
  sqlite3_stmt* StmtDeleteEdges = nullptr;

  // Telemetry members
  double CumulativeInsertNodeTime = 0;
  double CumulativeInsertEdgeTime = 0;
  double CumulativeDeleteTime = 0;

  // In-memory string interning caches (eliminates SQL JOINs and repeated SELECTs)
  std::unordered_map<std::string, int> FileToId;
  std::unordered_map<int, std::string> IdToFile;
  std::unordered_map<std::string, int> TypeToId;
  std::unordered_map<int, std::string> IdToType;

  // We keep an in-memory cache of Node objects returned by
  // GetNode/FindNodeByLocation so that QueryEngine can receive stable pointers
  // (like it did with Graph). The cache is cleared when connection is closed or
  // deleted.
  std::unordered_map<Node::Id, std::unique_ptr<Node>> NodeCache;

  int GetOrCreateFileId(const std::string& Path);
  int GetFileId(const std::string& Path);
  const std::string& GetFileById(int Id) const;
  int GetOrCreateTypeId(const std::string& TypeName);
  const std::string& GetTypeById(int Id) const;
  void LoadInternTables();

  std::unique_ptr<Node> ParseNodeFromRow(sqlite3_stmt* Stmt);
  Edge ParseEdgeFromRow(sqlite3_stmt* Stmt);

  bool PrepareStatements();
  void FinalizeStatements();
  bool ExecuteSimpleSql(const std::string& Sql);
};

}  // namespace dfg

#endif  // DATABASE_H
