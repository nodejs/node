// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "database.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>

namespace dfg {

static int64_t GetFileMtimeNs(const std::string& Path) {
  struct stat St;
  if (stat(Path.c_str(), &St) != 0) return -1;
#ifdef __APPLE__
  return static_cast<int64_t>(St.st_mtimespec.tv_sec) * 1000000000LL +
         St.st_mtimespec.tv_nsec;
#else
  return static_cast<int64_t>(St.st_mtim.tv_sec) * 1000000000LL +
         St.st_mtim.tv_nsec;
#endif
}

Database::Database() {}

Database::~Database() { Close(); }

bool Database::ExecuteSimpleSql(const std::string& Sql) {
  char* ErrorMsg = nullptr;
  int Rc = sqlite3_exec(Db, Sql.c_str(), nullptr, nullptr, &ErrorMsg);
  if (Rc != SQLITE_OK) {
    std::cerr << "[Database] SQLite Exec Error (" << Sql
              << "): " << (ErrorMsg ? ErrorMsg : "unknown") << std::endl;
    if (ErrorMsg) sqlite3_free(ErrorMsg);
    return false;
  }
  return true;
}

bool Database::Open(const std::string& DbPath) {
  int64_t InitialDbMtime = -1;
  if (!DbPath.empty() && DbPath != ":memory:") {
    InitialDbMtime = std::max(GetFileMtimeNs(DbPath),
                              GetFileMtimeNs(DbPath + "-wal"));
  }

  int Rc = sqlite3_open(DbPath.c_str(), &Db);
  if (Rc != SQLITE_OK) {
    std::cerr << "[Database] Failed to open SQLite database at " << DbPath
              << ": " << sqlite3_errmsg(Db) << std::endl;
    Close();
    return false;
  }

  std::cerr << "[Database] Connected to SQLite database: " << DbPath
            << std::endl;

  if (!ExecuteSimpleSql("PRAGMA journal_mode = WAL;")) {
    Close();
    return false;
  }
  if (!ExecuteSimpleSql("PRAGMA synchronous = NORMAL;")) {
    Close();
    return false;
  }
  if (!ExecuteSimpleSql("PRAGMA mmap_size = 268435456;")) {
    Close();
    return false;
  }
  if (!ExecuteSimpleSql("PRAGMA cache_size = -200000;")) {
    Close();
    return false;
  }

  // Foreign keys must be OFF: cross-TU edges reference node IDs declared in
  // headers or other TUs that may be deleted/re-inserted during incremental
  // DeleteFileData(file_id) updates, or unindexed external declarations.
  if (!ExecuteSimpleSql("PRAGMA foreign_keys = OFF;")) {
    Close();
    return false;
  }

  std::string CreateTablesSql =
      "CREATE TABLE IF NOT EXISTS files (\n"
      "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
      "    path TEXT UNIQUE\n"
      ");\n"
      "CREATE TABLE IF NOT EXISTS types (\n"
      "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
      "    name TEXT UNIQUE\n"
      ");\n"
      "CREATE TABLE IF NOT EXISTS file_hashes (\n"
      "    file_id INTEGER PRIMARY KEY,\n"
      "    hash TEXT\n"
      ");\n"
      "CREATE TABLE IF NOT EXISTS nodes (\n"
      "    id INTEGER PRIMARY KEY,\n"
      "    kind INTEGER,\n"
      "    name TEXT,\n"
      "    type_id INTEGER,\n"
      "    file_id INTEGER,\n"
      "    line INTEGER,\n"
      "    column INTEGER\n"
      ");\n"
      "CREATE TABLE IF NOT EXISTS edges (\n"
      "    from_id INTEGER,\n"
      "    to_id INTEGER,\n"
      "    kind INTEGER,\n"
      "    file_id INTEGER,\n"
      "    line INTEGER,\n"
      "    column INTEGER,\n"
      "    PRIMARY KEY (from_id, to_id, kind, file_id, line, column)\n"
      ") WITHOUT ROWID;\n";

  if (!ExecuteSimpleSql(CreateTablesSql)) {
    Close();
    return false;
  }

  // Note: edges is clustered on (from_id, ...), so idx_edges_from is not needed
  // for WITHOUT ROWID tables; idx_edges_to is automatically a covering index.
  std::string CreateIndicesSql =
      "CREATE INDEX IF NOT EXISTS idx_nodes_location ON nodes(file_id, line, "
      "column);\n"
      "CREATE INDEX IF NOT EXISTS idx_edges_to ON edges(to_id);\n"
      "CREATE INDEX IF NOT EXISTS idx_edges_file_line ON edges(file_id, line, "
      "column);\n";

  if (!ExecuteSimpleSql(CreateIndicesSql)) {
    std::cerr << "[Database] Legacy schema detected; resetting tables to v2 "
                 "normalized schema."
              << std::endl;
    std::string DropLegacySql =
        "DROP INDEX IF EXISTS idx_edges_from;\n"
        "DROP TABLE IF EXISTS edges;\n"
        "DROP TABLE IF EXISTS nodes;\n"
        "DROP TABLE IF EXISTS file_hashes;\n"
        "DROP TABLE IF EXISTS files;\n"
        "DROP TABLE IF EXISTS types;\n";
    if (!ExecuteSimpleSql(DropLegacySql) ||
        !ExecuteSimpleSql(CreateTablesSql) ||
        !ExecuteSimpleSql(CreateIndicesSql)) {
      Close();
      return false;
    }
  }

  if (!PrepareStatements()) {
    Close();
    return false;
  }

  CurrentDbPath = DbPath;
  LoadInternTables();

  if (!CurrentDbPath.empty() && CurrentDbPath != ":memory:") {
    std::string CsrPath = CurrentDbPath + ".csr";
    int64_t CsrMtime = GetFileMtimeNs(CsrPath);
    if (CsrMtime > 0 && CsrMtime + 5000000000LL >= InitialDbMtime) {
      Csr.Open(CsrPath);
    }
  }
  return true;
}

bool Database::BuildAndLoadCsrIndex() {
  if (CurrentDbPath.empty() || CurrentDbPath == ":memory:") return false;
  if (Db) {
    sqlite3_wal_checkpoint_v2(Db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr,
                              nullptr);
  }
  Csr.Close();
  std::vector<Node> AllNodes;
  std::vector<Edge> AllEdges;
  if (!DumpAll(AllNodes, AllEdges)) return false;
  std::string CsrPath = CurrentDbPath + ".csr";
  if (!CsrIndex::Build(CsrPath, AllNodes, AllEdges)) return false;
  return Csr.Open(CsrPath);
}

void Database::Close() {
  Csr.Close();
  std::string SavedCsrPath =
      CurrentDbPath.empty() ? "" : (CurrentDbPath + ".csr");
  CurrentDbPath.clear();
  FinalizeStatements();
  NodeCache.clear();
  FileToId.clear();
  IdToFile.clear();
  TypeToId.clear();
  IdToType.clear();
  if (Db) {
    sqlite3_close(Db);
    Db = nullptr;
    if (!SavedCsrPath.empty()) {
      utimensat(AT_FDCWD, SavedCsrPath.c_str(), nullptr, 0);
    }
    std::cerr << "[Database] Disconnected from SQLite database." << std::endl;
  }
}

void Database::LoadInternTables() {
  FileToId.clear();
  IdToFile.clear();
  TypeToId.clear();
  IdToType.clear();

  sqlite3_stmt* Stmt = nullptr;
  if (sqlite3_prepare_v2(Db, "SELECT id, path FROM files;", -1, &Stmt,
                         nullptr) == SQLITE_OK) {
    while (sqlite3_step(Stmt) == SQLITE_ROW) {
      int Id = sqlite3_column_int(Stmt, 0);
      const char* Path =
          reinterpret_cast<const char*>(sqlite3_column_text(Stmt, 1));
      std::string PathStr = Path ? Path : "";
      FileToId[PathStr] = Id;
      IdToFile[Id] = std::move(PathStr);
    }
    sqlite3_finalize(Stmt);
  }

  if (sqlite3_prepare_v2(Db, "SELECT id, name FROM types;", -1, &Stmt,
                         nullptr) == SQLITE_OK) {
    while (sqlite3_step(Stmt) == SQLITE_ROW) {
      int Id = sqlite3_column_int(Stmt, 0);
      const char* Name =
          reinterpret_cast<const char*>(sqlite3_column_text(Stmt, 1));
      std::string NameStr = Name ? Name : "";
      TypeToId[NameStr] = Id;
      IdToType[Id] = std::move(NameStr);
    }
    sqlite3_finalize(Stmt);
  }
}

int Database::GetFileId(const std::string& Path) {
  auto it = FileToId.find(Path);
  if (it != FileToId.end()) {
    return it->second;
  }

  sqlite3_reset(StmtGetFileId);
  sqlite3_bind_text(StmtGetFileId, 1, Path.c_str(), -1, SQLITE_STATIC);
  int Id = -1;
  if (sqlite3_step(StmtGetFileId) == SQLITE_ROW) {
    Id = sqlite3_column_int(StmtGetFileId, 0);
  }
  sqlite3_reset(StmtGetFileId);
  if (Id != -1) {
    FileToId[Path] = Id;
    IdToFile[Id] = Path;
  }
  return Id;
}

int Database::GetOrCreateFileId(const std::string& Path) {
  int Id = GetFileId(Path);
  if (Id != -1) return Id;

  sqlite3_reset(StmtInsertFile);
  sqlite3_bind_text(StmtInsertFile, 1, Path.c_str(), -1, SQLITE_STATIC);
  sqlite3_step(StmtInsertFile);
  sqlite3_reset(StmtInsertFile);

  return GetFileId(Path);
}

const std::string& Database::GetFileById(int Id) const {
  static const std::string Empty;
  auto it = IdToFile.find(Id);
  return it != IdToFile.end() ? it->second : Empty;
}

int Database::GetOrCreateTypeId(const std::string& TypeName) {
  auto it = TypeToId.find(TypeName);
  if (it != TypeToId.end()) {
    return it->second;
  }

  sqlite3_reset(StmtGetTypeId);
  sqlite3_bind_text(StmtGetTypeId, 1, TypeName.c_str(), -1, SQLITE_STATIC);
  int Id = -1;
  if (sqlite3_step(StmtGetTypeId) == SQLITE_ROW) {
    Id = sqlite3_column_int(StmtGetTypeId, 0);
  }
  sqlite3_reset(StmtGetTypeId);

  if (Id == -1) {
    sqlite3_reset(StmtInsertType);
    sqlite3_bind_text(StmtInsertType, 1, TypeName.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(StmtInsertType);
    sqlite3_reset(StmtInsertType);

    sqlite3_reset(StmtGetTypeId);
    sqlite3_bind_text(StmtGetTypeId, 1, TypeName.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(StmtGetTypeId) == SQLITE_ROW) {
      Id = sqlite3_column_int(StmtGetTypeId, 0);
    }
    sqlite3_reset(StmtGetTypeId);
  }

  if (Id != -1) {
    TypeToId[TypeName] = Id;
    IdToType[Id] = TypeName;
  }
  return Id;
}

const std::string& Database::GetTypeById(int Id) const {
  static const std::string Empty;
  auto it = IdToType.find(Id);
  return it != IdToType.end() ? it->second : Empty;
}

bool Database::PrepareStatements() {
#define PREPARE_STMT(sql, stmt)                                    \
  Rc = sqlite3_prepare_v2(Db, sql, -1, &stmt, nullptr);            \
  if (Rc != SQLITE_OK) {                                           \
    std::cerr << "[Database] Failed to prepare statement: " << sql \
              << ". Error: " << sqlite3_errmsg(Db) << std::endl;   \
    return false;                                                  \
  }

  int Rc;
  PREPARE_STMT("INSERT OR IGNORE INTO files (path) VALUES (?);", StmtInsertFile);
  PREPARE_STMT("SELECT id FROM files WHERE path = ?;", StmtGetFileId);
  PREPARE_STMT("INSERT OR IGNORE INTO types (name) VALUES (?);", StmtInsertType);
  PREPARE_STMT("SELECT id FROM types WHERE name = ?;", StmtGetTypeId);
  PREPARE_STMT(
      "INSERT INTO file_hashes (file_id, hash) VALUES (?, ?) ON "
      "CONFLICT(file_id) DO UPDATE SET hash = excluded.hash;",
      StmtUpdateHash);
  PREPARE_STMT("SELECT hash FROM file_hashes WHERE file_id = ?;", StmtGetHash);
  PREPARE_STMT("DELETE FROM file_hashes WHERE file_id = ?;", StmtDeleteHash);
  PREPARE_STMT("DELETE FROM edges WHERE file_id = ?;", StmtDeleteEdges);
  PREPARE_STMT("DELETE FROM nodes WHERE file_id = ?;", StmtDeleteNodes);
  PREPARE_STMT(
      "INSERT OR IGNORE INTO nodes (id, kind, name, type_id, file_id, line, "
      "column) VALUES (?, ?, ?, ?, ?, ?, ?);",
      StmtInsertNode);
  PREPARE_STMT(
      "INSERT OR IGNORE INTO edges (from_id, to_id, kind, file_id, line, "
      "column) VALUES (?, ?, ?, ?, ?, ?);",
      StmtInsertEdge);
  PREPARE_STMT(
      "SELECT id, kind, name, type_id, file_id, line, column FROM nodes WHERE "
      "id = ?;",
      StmtGetNode);
  PREPARE_STMT(
      "SELECT from_id, to_id, kind, file_id, line, column FROM edges WHERE "
      "to_id = ?;",
      StmtGetIncoming);
  PREPARE_STMT(
      "SELECT from_id, to_id, kind, file_id, line, column FROM edges WHERE "
      "from_id = ?;",
      StmtGetOutgoing);
  static_assert(
      static_cast<int>(EdgeKind::Implicit) == 6,
      "StmtHasOutgoingImplicit SQL query assumes EdgeKind::Implicit == 6");
  PREPARE_STMT(
      "SELECT 1 FROM edges WHERE from_id = ? AND kind = 6 LIMIT 1;",
      StmtHasOutgoingImplicit);
  PREPARE_STMT(
      "SELECT id, kind, name, type_id, file_id, line, column FROM nodes "
      "WHERE file_id = ? AND line = ? AND column <= ? "
      "ORDER BY column DESC LIMIT 1;",
      StmtFindNode);

#undef PREPARE_STMT
  return true;
}

void Database::FinalizeStatements() {
#define FINALIZE_STMT(stmt) \
  if (stmt) {               \
    sqlite3_finalize(stmt); \
    stmt = nullptr;         \
  }
  FINALIZE_STMT(StmtInsertFile);
  FINALIZE_STMT(StmtGetFileId);
  FINALIZE_STMT(StmtInsertType);
  FINALIZE_STMT(StmtGetTypeId);
  FINALIZE_STMT(StmtUpdateHash);
  FINALIZE_STMT(StmtGetHash);
  FINALIZE_STMT(StmtDeleteHash);
  FINALIZE_STMT(StmtDeleteEdges);
  FINALIZE_STMT(StmtDeleteNodes);
  FINALIZE_STMT(StmtInsertNode);
  FINALIZE_STMT(StmtInsertEdge);
  FINALIZE_STMT(StmtGetNode);
  FINALIZE_STMT(StmtGetIncoming);
  FINALIZE_STMT(StmtGetOutgoing);
  FINALIZE_STMT(StmtHasOutgoingImplicit);
  FINALIZE_STMT(StmtFindNode);
#undef FINALIZE_STMT
}

bool Database::BeginTransaction() {
  return ExecuteSimpleSql("BEGIN TRANSACTION;");
}

bool Database::CommitTransaction() { return ExecuteSimpleSql("COMMIT;"); }

bool Database::RollbackTransaction() { return ExecuteSimpleSql("ROLLBACK;"); }

std::string Database::GetFileHash(const std::string& File) {
  int FileId = GetFileId(File);
  if (FileId == -1) return "";

  sqlite3_reset(StmtGetHash);
  sqlite3_bind_int(StmtGetHash, 1, FileId);

  std::string Hash = "";
  int Rc = sqlite3_step(StmtGetHash);
  if (Rc == SQLITE_ROW) {
    const unsigned char* text = sqlite3_column_text(StmtGetHash, 0);
    if (text) {
      Hash = reinterpret_cast<const char*>(text);
    }
  }
  sqlite3_reset(StmtGetHash);
  return Hash;
}

bool Database::UpdateFileHash(const std::string& File,
                              const std::string& Hash) {
  int FileId = GetOrCreateFileId(File);
  if (FileId == -1) return false;

  sqlite3_reset(StmtUpdateHash);
  sqlite3_bind_int(StmtUpdateHash, 1, FileId);
  sqlite3_bind_text(StmtUpdateHash, 2, Hash.c_str(), -1, SQLITE_STATIC);

  int Rc = sqlite3_step(StmtUpdateHash);
  sqlite3_reset(StmtUpdateHash);
  return Rc == SQLITE_DONE;
}

bool Database::DeleteFileData(const std::string& File) {
  int FileId = GetFileId(File);
  if (FileId == -1) return true;

  std::chrono::high_resolution_clock::time_point Start;
  if (EnableTelemetry) Start = std::chrono::high_resolution_clock::now();

  sqlite3_reset(StmtDeleteHash);
  sqlite3_bind_int(StmtDeleteHash, 1, FileId);
  int Rc0 = sqlite3_step(StmtDeleteHash);
  sqlite3_reset(StmtDeleteHash);

  sqlite3_reset(StmtDeleteEdges);
  sqlite3_bind_int(StmtDeleteEdges, 1, FileId);
  int Rc1 = sqlite3_step(StmtDeleteEdges);
  sqlite3_reset(StmtDeleteEdges);

  sqlite3_reset(StmtDeleteNodes);
  sqlite3_bind_int(StmtDeleteNodes, 1, FileId);
  int Rc2 = sqlite3_step(StmtDeleteNodes);
  sqlite3_reset(StmtDeleteNodes);

  for (auto it = NodeCache.begin(); it != NodeCache.end();) {
    if (it->second->Loc.File == File) {
      it = NodeCache.erase(it);
    } else {
      ++it;
    }
  }

  if (EnableTelemetry) {
    auto End = std::chrono::high_resolution_clock::now();
    CumulativeDeleteTime += std::chrono::duration<double>(End - Start).count();
  }
  if (Csr.IsOpen()) Csr.Close();
  return Rc0 == SQLITE_DONE && Rc1 == SQLITE_DONE && Rc2 == SQLITE_DONE;
}

bool Database::DeleteFilesData(const std::vector<std::string>& Files) {
  if (Files.empty()) return true;
  if (Files.size() == 1) return DeleteFileData(Files[0]);

  std::vector<int> FileIds;
  std::unordered_set<std::string> FileSet(Files.begin(), Files.end());
  FileIds.reserve(Files.size());
  for (const auto& F : Files) {
    int Id = GetFileId(F);
    if (Id != -1) {
      FileIds.push_back(Id);
    }
  }
  if (FileIds.empty()) return true;

  std::chrono::high_resolution_clock::time_point Start;
  if (EnableTelemetry) Start = std::chrono::high_resolution_clock::now();

  std::string InList;
  for (size_t i = 0; i < FileIds.size(); ++i) {
    if (i > 0) InList += ",";
    InList += std::to_string(FileIds[i]);
  }

  bool Ok = ExecuteSimpleSql("DELETE FROM file_hashes WHERE file_id IN (" +
                             InList + ");") &&
            ExecuteSimpleSql("DELETE FROM edges WHERE file_id IN (" + InList +
                             ");") &&
            ExecuteSimpleSql("DELETE FROM nodes WHERE file_id IN (" + InList +
                             ");");

  for (auto it = NodeCache.begin(); it != NodeCache.end();) {
    if (FileSet.find(it->second->Loc.File) != FileSet.end()) {
      it = NodeCache.erase(it);
    } else {
      ++it;
    }
  }

  if (EnableTelemetry) {
    auto End = std::chrono::high_resolution_clock::now();
    CumulativeDeleteTime += std::chrono::duration<double>(End - Start).count();
  }
  if (Csr.IsOpen()) Csr.Close();
  return Ok;
}

bool Database::InsertNode(const Node& N) {
  if (N.Loc.File.empty()) return true;
  if (Csr.IsOpen()) Csr.Close();

  std::chrono::high_resolution_clock::time_point Start;
  if (EnableTelemetry) Start = std::chrono::high_resolution_clock::now();

  int TypeId = GetOrCreateTypeId(N.Type);
  int FileId = GetOrCreateFileId(N.Loc.File);

  sqlite3_reset(StmtInsertNode);
  sqlite3_bind_int64(StmtInsertNode, 1, static_cast<sqlite3_int64>(N.ID));
  sqlite3_bind_int(StmtInsertNode, 2, static_cast<int>(N.Kind));
  sqlite3_bind_text(StmtInsertNode, 3, N.Name.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(StmtInsertNode, 4, TypeId);
  sqlite3_bind_int(StmtInsertNode, 5, FileId);
  sqlite3_bind_int(StmtInsertNode, 6, N.Loc.Line);
  sqlite3_bind_int(StmtInsertNode, 7, N.Loc.Column);

  int Rc = sqlite3_step(StmtInsertNode);
  sqlite3_reset(StmtInsertNode);

  if (EnableTelemetry) {
    auto End = std::chrono::high_resolution_clock::now();
    CumulativeInsertNodeTime +=
        std::chrono::duration<double>(End - Start).count();
  }
  return Rc == SQLITE_DONE || Rc == SQLITE_OK;
}

bool Database::InsertEdge(const Edge& E) {
  if (E.Loc.File.empty()) return true;
  if (Csr.IsOpen()) Csr.Close();

  std::chrono::high_resolution_clock::time_point Start;
  if (EnableTelemetry) Start = std::chrono::high_resolution_clock::now();

  int FileId = GetOrCreateFileId(E.Loc.File);

  sqlite3_reset(StmtInsertEdge);
  sqlite3_bind_int64(StmtInsertEdge, 1, static_cast<sqlite3_int64>(E.From));
  sqlite3_bind_int64(StmtInsertEdge, 2, static_cast<sqlite3_int64>(E.To));
  sqlite3_bind_int(StmtInsertEdge, 3, static_cast<int>(E.Kind));
  sqlite3_bind_int(StmtInsertEdge, 4, FileId);
  sqlite3_bind_int(StmtInsertEdge, 5, E.Loc.Line);
  sqlite3_bind_int(StmtInsertEdge, 6, E.Loc.Column);

  int Rc = sqlite3_step(StmtInsertEdge);
  sqlite3_reset(StmtInsertEdge);

  if (EnableTelemetry) {
    auto End = std::chrono::high_resolution_clock::now();
    CumulativeInsertEdgeTime +=
        std::chrono::duration<double>(End - Start).count();
  }
  return Rc == SQLITE_DONE || Rc == SQLITE_OK;
}

static std::string SafeGetColumnText(sqlite3_stmt* Stmt, int Col) {
  const char* Text =
      reinterpret_cast<const char*>(sqlite3_column_text(Stmt, Col));
  return Text ? Text : "";
}

std::unique_ptr<Node> Database::ParseNodeFromRow(sqlite3_stmt* Stmt) {
  auto N = std::make_unique<Node>();
  N->ID = static_cast<uint64_t>(sqlite3_column_int64(Stmt, 0));
  N->Kind = static_cast<NodeKind>(sqlite3_column_int(Stmt, 1));
  N->Name = SafeGetColumnText(Stmt, 2);
  N->Type = GetTypeById(sqlite3_column_int(Stmt, 3));
  N->Loc.File = GetFileById(sqlite3_column_int(Stmt, 4));
  N->Loc.Line = sqlite3_column_int(Stmt, 5);
  N->Loc.Column = sqlite3_column_int(Stmt, 6);
  return N;
}

Edge Database::ParseEdgeFromRow(sqlite3_stmt* Stmt) {
  Edge E;
  E.From = static_cast<uint64_t>(sqlite3_column_int64(Stmt, 0));
  E.To = static_cast<uint64_t>(sqlite3_column_int64(Stmt, 1));
  E.Kind = static_cast<EdgeKind>(sqlite3_column_int(Stmt, 2));
  E.Loc.File = GetFileById(sqlite3_column_int(Stmt, 3));
  E.Loc.Line = sqlite3_column_int(Stmt, 4);
  E.Loc.Column = sqlite3_column_int(Stmt, 5);
  return E;
}

const Node* Database::GetNode(const Node::Id& Id) {
  auto it = NodeCache.find(Id);
  if (it != NodeCache.end()) {
    return it->second.get();
  }

  if (Csr.IsOpen()) {
    auto N = Csr.GetNodeById(Id);
    if (!N) return nullptr;
    const Node* Ptr = N.get();
    NodeCache[Id] = std::move(N);
    return Ptr;
  }

  sqlite3_reset(StmtGetNode);
  sqlite3_bind_int64(StmtGetNode, 1, static_cast<sqlite3_int64>(Id));

  int Rc = sqlite3_step(StmtGetNode);
  const Node* ptr = nullptr;
  if (Rc == SQLITE_ROW) {
    auto N = ParseNodeFromRow(StmtGetNode);
    Node::Id CachedId = N->ID;
    ptr = N.get();
    NodeCache[CachedId] = std::move(N);
  }
  sqlite3_reset(StmtGetNode);
  return ptr;
}

static std::string ExtractIdentifierTokenAt(const std::string& CurrentDbPath,
                                            const std::string& File,
                                            unsigned Line, unsigned Col,
                                            unsigned& OutStartCol) {
  OutStartCol = Col;
  if (Line == 0 || Col == 0) return "";

  std::filesystem::path Candidate(File);
  if (!std::filesystem::exists(Candidate) && !CurrentDbPath.empty()) {
    std::filesystem::path DbDir =
        std::filesystem::path(CurrentDbPath).parent_path();
    if (std::filesystem::exists(DbDir / File)) {
      Candidate = DbDir / File;
    } else if (DbDir.has_parent_path() &&
               std::filesystem::exists(DbDir.parent_path() / File)) {
      Candidate = DbDir.parent_path() / File;
    }
  }
  std::ifstream In(Candidate);
  if (!In.is_open()) return "";

  std::string LineStr;
  for (unsigned CurLine = 1; CurLine <= Line; ++CurLine) {
    if (!std::getline(In, LineStr)) return "";
  }
  if (LineStr.empty()) return "";

  auto IsIdentChar = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
  };
  size_t Idx = (Col - 1 < LineStr.size()) ? (Col - 1) : (LineStr.size() - 1);
  if (!IsIdentChar(LineStr[Idx])) {
    if (Idx + 1 < LineStr.size() && IsIdentChar(LineStr[Idx + 1])) {
      ++Idx;
    } else if (Idx > 0 && IsIdentChar(LineStr[Idx - 1])) {
      --Idx;
    } else {
      return "";
    }
  }
  size_t Start = Idx;
  while (Start > 0 && IsIdentChar(LineStr[Start - 1])) {
    --Start;
  }
  size_t End = Idx + 1;
  while (End < LineStr.size() && IsIdentChar(LineStr[End])) {
    ++End;
  }
  OutStartCol = static_cast<unsigned>(Start + 1);
  return LineStr.substr(Start, End - Start);
}

const Node* Database::FindNodeByLocation(const std::string& File, unsigned Line,
                                         unsigned Col) {
  unsigned TokenStartCol = Col;
  std::string TokenName =
      ExtractIdentifierTokenAt(CurrentDbPath, File, Line, Col, TokenStartCol);
  std::string TokenCallName = TokenName.empty() ? "" : (TokenName + "()");
  std::string TokenReturnName =
      TokenName.empty() ? "" : (TokenName + "_return");

  auto CacheAndReturn = [this](Node&& Found) -> const Node* {
    Node::Id CachedId = Found.ID;
    auto it = NodeCache.find(CachedId);
    if (it != NodeCache.end()) {
      return it->second.get();
    }
    auto N = std::make_unique<Node>(std::move(Found));
    const Node* Ptr = N.get();
    NodeCache[CachedId] = std::move(N);
    return Ptr;
  };

  if (Csr.IsOpen()) {
    auto Matches = Csr.FindNodesAtLocation(File, Line, Col);
    int BestIdx = -1;
    int BestScore = -1000;
    std::string QualifiedCallSuffix =
        TokenCallName.empty() ? "" : ("::" + TokenCallName);
    auto MatchesToken = [&](const std::string& Name) {
      if (TokenName.empty()) return false;
      if (Name == TokenName || Name == TokenCallName ||
          Name == TokenReturnName) {
        return true;
      }
      if (!QualifiedCallSuffix.empty() &&
          Name.size() >= QualifiedCallSuffix.size() &&
          Name.compare(Name.size() - QualifiedCallSuffix.size(),
                       QualifiedCallSuffix.size(), QualifiedCallSuffix) == 0) {
        return true;
      }
      return false;
    };
    for (int i = 0; i < static_cast<int>(Matches.size()); ++i) {
      const auto& M = Matches[i];
      if (M.Name.empty()) continue;
      int Score = 0;
      if (MatchesToken(M.Name)) {
        Score += 1000;
      }
      if (M.Name != "call()" && M.Name != "expr") {
        Score += 200;
      }
      if (!Csr.GetIncomingEdges(M.ID).empty()) {
        Score += 100;
      }
      if (!Csr.GetOutgoingEdges(M.ID).empty()) {
        Score += 100;
      }
      if (M.Kind == NodeKind::Parameter) {
        if (Csr.HasOutgoingImplicitEdge(M.ID)) {
          Score += 50;
        }
        int LiveCallerCount = 0;
        for (const auto& E : Csr.GetIncomingEdges(M.ID)) {
          if (E.Kind == EdgeKind::ArgPass) {
            ++LiveCallerCount;
          } else if (E.Kind == EdgeKind::Implicit) {
            for (const auto& PE : Csr.GetIncomingEdges(E.From)) {
              if (PE.Kind == EdgeKind::ArgPass) {
                ++LiveCallerCount;
              }
            }
          }
        }
        if (LiveCallerCount == 0) {
          for (const auto& E : Csr.GetOutgoingEdges(M.ID)) {
            if (E.Kind == EdgeKind::Implicit) {
              for (const auto& PE : Csr.GetIncomingEdges(E.To)) {
                if (PE.Kind == EdgeKind::ArgPass) {
                  ++LiveCallerCount;
                }
              }
            }
          }
        }
        if (LiveCallerCount > 0) {
          Score += 80 + std::min(LiveCallerCount, 40);
        }
      } else if (M.Kind == NodeKind::Expression &&
                 M.Name.find("_return") != std::string::npos) {
        int ReturnCallerCount = 0;
        for (const auto& E : Csr.GetOutgoingEdges(M.ID)) {
          if (E.Kind == EdgeKind::Return) {
            ++ReturnCallerCount;
          }
        }
        if (ReturnCallerCount == 0) {
          for (const auto& E : Csr.GetIncomingEdges(M.ID)) {
            if (E.Kind == EdgeKind::Implicit) {
              for (const auto& PE : Csr.GetOutgoingEdges(E.From)) {
                if (PE.Kind == EdgeKind::Return) {
                  ++ReturnCallerCount;
                }
              }
            }
          }
        }
        if (ReturnCallerCount > 0) {
          Score += 80 + std::min(ReturnCallerCount, 40);
        }
      }
      if (Score > BestScore) {
        BestScore = Score;
        BestIdx = i;
      }
    }

    bool MatchedTokenDirectly =
        (BestIdx >= 0 && MatchesToken(Matches[BestIdx].Name));

    if (!TokenName.empty() && !MatchedTokenDirectly) {
      if (auto NamedNode = Csr.FindNodeByNameInFile(File, Line, TokenName)) {
        return CacheAndReturn(std::move(*NamedNode));
      }
    }

    if (BestIdx >= 0) {
      return CacheAndReturn(std::move(Matches[BestIdx]));
    }
    return nullptr;
  }

  int FileId = GetFileId(File);
  if (FileId == -1) return nullptr;

  sqlite3_reset(StmtFindNode);
  sqlite3_bind_int(StmtFindNode, 1, FileId);
  sqlite3_bind_int(StmtFindNode, 2, Line);
  sqlite3_bind_int(StmtFindNode, 3, Col);

  std::unique_ptr<Node> BestNode;
  int BestScore = -1000;
  unsigned FirstMatchedCol = 0;
  while (sqlite3_step(StmtFindNode) == SQLITE_ROW) {
    auto N = ParseNodeFromRow(StmtFindNode);
    if (!BestNode) {
      FirstMatchedCol = N->Loc.Column;
    } else if (N->Loc.Column != FirstMatchedCol) {
      break;
    }
    int Score = 0;
    if (!TokenName.empty() &&
        (N->Name == TokenName || N->Name == TokenCallName ||
         N->Name == TokenReturnName)) {
      Score += 100;
    }
    if (N->Name != "call()" && N->Name != "expr") {
      Score += 20;
    }
    if (!GetIncomingEdges(N->ID).empty()) {
      Score += 10;
    }
    if (!GetOutgoingEdges(N->ID).empty()) {
      Score += 10;
    }
    if (Score > BestScore) {
      BestScore = Score;
      BestNode = std::move(N);
    }
  }
  sqlite3_reset(StmtFindNode);

  bool MatchedTokenDirectly =
      (BestNode && !TokenName.empty() &&
       (BestNode->Name == TokenName || BestNode->Name == TokenCallName ||
        BestNode->Name == TokenReturnName));

  if (!TokenName.empty() && !MatchedTokenDirectly) {
    sqlite3_stmt* StmtByName = nullptr;
    const char* Sql =
        "SELECT id, kind, name, type_id, file_id, line, column FROM nodes "
        "WHERE file_id = ? AND (name = ? OR name = ?) "
        "ORDER BY CASE WHEN line <= ? THEN 0 ELSE 1 END, ABS(line - ?) ASC;";
    if (sqlite3_prepare_v2(Db, Sql, -1, &StmtByName, nullptr) == SQLITE_OK) {
      sqlite3_bind_int(StmtByName, 1, FileId);
      sqlite3_bind_text(StmtByName, 2, TokenName.c_str(), -1, SQLITE_STATIC);
      sqlite3_bind_text(StmtByName, 3, TokenCallName.c_str(), -1,
                        SQLITE_STATIC);
      sqlite3_bind_int(StmtByName, 4, Line);
      sqlite3_bind_int(StmtByName, 5, Line);
      while (sqlite3_step(StmtByName) == SQLITE_ROW) {
        auto Cand = ParseNodeFromRow(StmtByName);
        if (Cand->Kind == NodeKind::Parameter &&
            HasOutgoingImplicitEdge(Cand->ID)) {
          continue;
        }
        sqlite3_finalize(StmtByName);
        return CacheAndReturn(std::move(*Cand));
      }
      sqlite3_finalize(StmtByName);
    }
  }

  if (BestNode) {
    return CacheAndReturn(std::move(*BestNode));
  }
  return nullptr;
}

std::vector<Node> Database::FindNodesAtLocation(const std::string& File,
                                                unsigned Line, unsigned Col) {
  if (Csr.IsOpen()) {
    return Csr.FindNodesAtLocation(File, Line, Col);
  }
  std::vector<Node> Result;
  int FileId = GetFileId(File);
  if (FileId == -1) return Result;

  sqlite3_reset(StmtFindNode);
  sqlite3_bind_int(StmtFindNode, 1, FileId);
  sqlite3_bind_int(StmtFindNode, 2, Line);
  sqlite3_bind_int(StmtFindNode, 3, Col);

  unsigned FirstMatchedCol = 0;
  while (sqlite3_step(StmtFindNode) == SQLITE_ROW) {
    auto N = ParseNodeFromRow(StmtFindNode);
    if (Result.empty()) {
      FirstMatchedCol = N->Loc.Column;
    } else if (N->Loc.Column != FirstMatchedCol) {
      break;
    }
    Result.push_back(std::move(*N));
  }
  sqlite3_reset(StmtFindNode);
  return Result;
}

std::vector<Edge> Database::GetIncomingEdges(const Node::Id& ToId) {
  if (Csr.IsOpen()) {
    return Csr.GetIncomingEdges(ToId);
  }

  sqlite3_reset(StmtGetIncoming);
  sqlite3_bind_int64(StmtGetIncoming, 1, static_cast<sqlite3_int64>(ToId));

  std::vector<Edge> Result;
  while (sqlite3_step(StmtGetIncoming) == SQLITE_ROW) {
    Result.push_back(ParseEdgeFromRow(StmtGetIncoming));
  }
  sqlite3_reset(StmtGetIncoming);
  return Result;
}

std::vector<Edge> Database::GetOutgoingEdges(const Node::Id& FromId) {
  if (Csr.IsOpen()) {
    return Csr.GetOutgoingEdges(FromId);
  }

  sqlite3_reset(StmtGetOutgoing);
  sqlite3_bind_int64(StmtGetOutgoing, 1, static_cast<sqlite3_int64>(FromId));

  std::vector<Edge> Result;
  while (sqlite3_step(StmtGetOutgoing) == SQLITE_ROW) {
    Result.push_back(ParseEdgeFromRow(StmtGetOutgoing));
  }
  sqlite3_reset(StmtGetOutgoing);
  return Result;
}

bool Database::GetStats(int& NodesCount, int& EdgesCount) {
  if (Csr.IsOpen()) {
    NodesCount = static_cast<int>(Csr.GetNumNodeRecords());
    EdgesCount = static_cast<int>(Csr.GetNumEdges());
    return true;
  }

  NodesCount = 0;
  EdgesCount = 0;

  sqlite3_stmt* StmtNodes = nullptr;
  sqlite3_stmt* StmtEdges = nullptr;

  int Rc1 = sqlite3_prepare_v2(Db, "SELECT COUNT(*) FROM nodes;", -1,
                               &StmtNodes, nullptr);
  int Rc2 = sqlite3_prepare_v2(Db, "SELECT COUNT(*) FROM edges;", -1,
                               &StmtEdges, nullptr);

  if (Rc1 == SQLITE_OK && sqlite3_step(StmtNodes) == SQLITE_ROW) {
    NodesCount = sqlite3_column_int(StmtNodes, 0);
  }
  if (Rc2 == SQLITE_OK && sqlite3_step(StmtEdges) == SQLITE_ROW) {
    EdgesCount = sqlite3_column_int(StmtEdges, 0);
  }

  if (StmtNodes) sqlite3_finalize(StmtNodes);
  if (StmtEdges) sqlite3_finalize(StmtEdges);

  return Rc1 == SQLITE_OK && Rc2 == SQLITE_OK;
}

bool Database::DumpAll(std::vector<Node>& OutNodes,
                       std::vector<Edge>& OutEdges) {
  OutNodes.clear();
  OutEdges.clear();

  sqlite3_stmt* StmtNodes = nullptr;
  sqlite3_stmt* StmtEdges = nullptr;

  int Rc1 = sqlite3_prepare_v2(
      Db, "SELECT id, kind, name, type_id, file_id, line, column FROM nodes;",
      -1, &StmtNodes, nullptr);
  int Rc2 = sqlite3_prepare_v2(
      Db, "SELECT from_id, to_id, kind, file_id, line, column FROM edges;", -1,
      &StmtEdges, nullptr);

  if (Rc1 == SQLITE_OK) {
    while (sqlite3_step(StmtNodes) == SQLITE_ROW) {
      auto N = ParseNodeFromRow(StmtNodes);
      OutNodes.push_back(std::move(*N));
    }
  }

  if (Rc2 == SQLITE_OK) {
    while (sqlite3_step(StmtEdges) == SQLITE_ROW) {
      OutEdges.push_back(ParseEdgeFromRow(StmtEdges));
    }
  }

  if (StmtNodes) sqlite3_finalize(StmtNodes);
  if (StmtEdges) sqlite3_finalize(StmtEdges);

  return Rc1 == SQLITE_OK && Rc2 == SQLITE_OK;
}

bool Database::MergeFrom(const std::string& OtherDbPath, bool EnableTelemetry) {
  if (Csr.IsOpen()) Csr.Close();
  auto Start = std::chrono::high_resolution_clock::now();
  std::cerr << "[Database] Merging data from " << OtherDbPath << "..."
            << std::endl;

  std::string AttachSql = "ATTACH DATABASE '" + OtherDbPath + "' AS temp_db;";

  if (!ExecuteSimpleSql(AttachSql)) {
    return false;
  }

  bool TxStarted = BeginTransaction();
  if (!TxStarted) {
    std::cerr << "[Database] WARNING: Failed to start transaction for merge."
              << std::endl;
  }

  bool Success = true;
  if (!ExecuteSimpleSql(
          "INSERT OR IGNORE INTO files (path) SELECT path FROM temp_db.files;")) {
    Success = false;
  }
  if (Success &&
      !ExecuteSimpleSql(
          "INSERT OR IGNORE INTO types (name) SELECT name FROM temp_db.types;")) {
    Success = false;
  }
  if (Success &&
      !ExecuteSimpleSql(
          "INSERT OR IGNORE INTO nodes (id, kind, name, type_id, file_id, line, column) "
          "SELECT n.id, n.kind, n.name, main_t.id, main_f.id, n.line, n.column "
          "FROM temp_db.nodes n "
          "JOIN temp_db.types temp_t ON n.type_id = temp_t.id "
          "JOIN main.types main_t ON temp_t.name = main_t.name "
          "JOIN temp_db.files temp_f ON n.file_id = temp_f.id "
          "JOIN main.files main_f ON temp_f.path = main_f.path;")) {
    Success = false;
  }
  if (Success &&
      !ExecuteSimpleSql(
          "INSERT OR IGNORE INTO edges (from_id, to_id, kind, file_id, line, column) "
          "SELECT e.from_id, e.to_id, e.kind, main_f.id, e.line, e.column "
          "FROM temp_db.edges e "
          "JOIN temp_db.files temp_f ON e.file_id = temp_f.id "
          "JOIN main.files main_f ON temp_f.path = main_f.path;")) {
    Success = false;
  }
  if (Success &&
      !ExecuteSimpleSql(
          "INSERT OR REPLACE INTO file_hashes (file_id, hash) "
          "SELECT main_f.id, fh.hash "
          "FROM temp_db.file_hashes fh "
          "JOIN temp_db.files temp_f ON fh.file_id = temp_f.id "
          "JOIN main.files main_f ON temp_f.path = main_f.path;")) {
    Success = false;
  }

  if (TxStarted) {
    if (Success) {
      CommitTransaction();
      NodeCache.clear();
      LoadInternTables();
      std::cerr << "[Database] Transaction committed for merge." << std::endl;
    } else {
      RollbackTransaction();
      std::cerr << "[Database] ERROR: Merge failed, transaction rolled back."
                << std::endl;
    }
  }

  if (!ExecuteSimpleSql("DETACH DATABASE temp_db;")) {
    std::cerr << "[Database] ERROR: Failed to detach database temp_db!"
              << std::endl;
    Success = false;
  } else {
    std::cerr << "[Database] Detached temp_db successfully." << std::endl;
  }

  auto End = std::chrono::high_resolution_clock::now();
  double Elapsed = std::chrono::duration<double>(End - Start).count();
  if (EnableTelemetry) {
    std::cerr << "[Telemetry] [Merge] Merged " << OtherDbPath << " in "
              << Elapsed << "s" << std::endl;
  }

  return Success;
}

bool Database::HasOutgoingImplicitEdge(uint64_t NodeId) {
  if (Csr.IsOpen()) {
    return Csr.HasOutgoingImplicitEdge(NodeId);
  }

  sqlite3_reset(StmtHasOutgoingImplicit);
  sqlite3_bind_int64(StmtHasOutgoingImplicit, 1,
                     static_cast<sqlite3_int64>(NodeId));
  bool Result = (sqlite3_step(StmtHasOutgoingImplicit) == SQLITE_ROW);
  sqlite3_reset(StmtHasOutgoingImplicit);
  return Result;
}

}  // namespace dfg
