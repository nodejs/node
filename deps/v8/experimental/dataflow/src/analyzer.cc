// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sqlite3.h>
#include <sys/resource.h>
#include <cstdlib>

#include <chrono>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <sstream>

#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "database.h"
#include "dfg.h"
#include "dfg_visitor.h"
#include "index.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "path_translator.h"
#include "server.h"
#include "wrapper_config.h"

using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory DataFlowAnalyzerCategory("dataflow-analyzer options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("\nMore help text...\n");

// Standard LLVM CommandLine Options for Server and Standalone Index modes
static cl::opt<bool> ServerMode("server",
                                cl::desc("Run in JSON-RPC server mode"),
                                cl::cat(DataFlowAnalyzerCategory));
static cl::opt<bool> IndexMode(
    "index", cl::desc("Run in standalone codebase indexing mode"),
    cl::cat(DataFlowAnalyzerCategory));
static cl::opt<std::string> WorkspaceRootOpt(
    "workspace-root", cl::desc("Workspace root directory for indexing"),
    cl::init("."), cl::cat(DataFlowAnalyzerCategory));
static cl::opt<std::string> CacheDirOpt(
    "cache-dir", cl::desc("Directory for cache database"),
    cl::cat(DataFlowAnalyzerCategory));
static cl::opt<std::string> CompileCommandsDirOpt(
    "compile-commands-dir",
    cl::desc("Directory containing compile_commands.json"),
    cl::cat(DataFlowAnalyzerCategory));
static cl::opt<unsigned> ConcurrencyOpt(
    "j", cl::desc("Number of parallel threads for indexing"), cl::init(0),
    cl::cat(DataFlowAnalyzerCategory));
static cl::opt<bool> MigrateMode(
    "migrate-db", cl::desc("Run in SQLite database schema migration mode"),
    cl::cat(DataFlowAnalyzerCategory));
static cl::opt<std::string> OldDbOpt(
    "old-db", cl::desc("Path to the existing old 54GB SQLite database file"),
    cl::cat(DataFlowAnalyzerCategory));
static cl::opt<std::string> NewDbOpt(
    "new-db", cl::desc("Path to the target optimized database file to write"),
    cl::cat(DataFlowAnalyzerCategory));
static cl::opt<std::string> WrapperConfigOpt(
    "wrapper-config", cl::desc("Path to C++ wrapper JSON configuration file"),
    cl::cat(DataFlowAnalyzerCategory));
static cl::opt<bool> TelemetryOpt(
    "telemetry", cl::desc("Enable detailed performance telemetry logging"),
    cl::cat(DataFlowAnalyzerCategory));

class DfgActionFactory : public FrontendActionFactory {
 public:
  explicit DfgActionFactory(dfg::DfgGraph& Graph,
                            const dfg::PathTranslator& Translator)
      : Graph(Graph), Translator(Translator) {}
  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<dfg::DfgAction>(Graph, Translator);
  }

 private:
  dfg::DfgGraph& Graph;
  const dfg::PathTranslator& Translator;
};

bool HasFlag(int argc, const char** argv, const std::string& Flag) {
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == Flag) {
      return true;
    }
  }
  return false;
}

// High-performance C++ SQLite schema migrator & optimizer
bool MigrateDatabase(const std::string& OldPath, const std::string& NewPath) {
  std::cout << "Starting C++ Database Migration & Compaction Tool..."
            << std::endl;
  std::cout << "  Source DB: " << OldPath << std::endl;
  std::cout << "  Target DB: " << NewPath << std::endl;

  if (llvm::sys::fs::exists(NewPath)) {
    llvm::sys::fs::remove(NewPath);
  }

  // Initialize target DB with normalized WITHOUT ROWID schema
  dfg::Database TargetDb;
  if (!TargetDb.Open(NewPath)) {
    std::cerr << "ERROR: Failed to initialize schema at " << NewPath
              << std::endl;
    return false;
  }
  TargetDb.Close();

  sqlite3* NewDb = nullptr;
  if (sqlite3_open(NewPath.c_str(), &NewDb) != SQLITE_OK) {
    std::cerr << "ERROR: Failed to open NewDb raw" << std::endl;
    return false;
  }

  auto ExecSql = [NewDb](const std::string& Sql) {
    char* Err = nullptr;
    int Rc = sqlite3_exec(NewDb, Sql.c_str(), nullptr, nullptr, &Err);
    if (Rc != SQLITE_OK) {
      std::cerr << "SQL Error: " << (Err ? Err : "unknown")
                << "\n  Query: " << Sql << std::endl;
      if (Err) sqlite3_free(Err);
      return false;
    }
    return true;
  };

  ExecSql("PRAGMA journal_mode = WAL;");
  ExecSql("PRAGMA synchronous = OFF;");
  ExecSql("PRAGMA foreign_keys = OFF;");

  static_assert(static_cast<int>(dfg::NodeKind::Expression) == 6,
                "MigrateDatabase SQL assumes NodeKind::Expression == 6");
  static_assert(static_cast<int>(dfg::EdgeKind::Initialization) == 2,
                "MigrateDatabase SQL assumes EdgeKind::Initialization == 2");
  static_assert(static_cast<int>(dfg::EdgeKind::ArgPass) == 3,
                "MigrateDatabase SQL assumes EdgeKind::ArgPass == 3");
  static_assert(static_cast<int>(dfg::EdgeKind::Return) == 4,
                "MigrateDatabase SQL assumes EdgeKind::Return == 4");
  static_assert(static_cast<int>(dfg::EdgeKind::Implicit) == 6,
                "MigrateDatabase SQL assumes EdgeKind::Implicit == 6");

  auto StartTime = std::chrono::steady_clock::now();

  if (!ExecSql("ATTACH DATABASE '" + OldPath + "' AS old_db;")) {
    sqlite3_close(NewDb);
    return false;
  }

  auto AbortMigration = [&]() {
    ExecSql("ROLLBACK;");
    ExecSql("DETACH DATABASE old_db;");
    sqlite3_close(NewDb);
    return false;
  };

  if (!ExecSql("BEGIN TRANSACTION;")) return AbortMigration();
  std::cout << "[1/4] Migrating non-empty files and types..." << std::endl;
  if (!ExecSql("INSERT INTO main.files (id, path) SELECT id, path FROM "
               "old_db.files WHERE path != '';") ||
      !ExecSql("INSERT INTO main.types (id, name) SELECT id, name FROM "
               "old_db.types;") ||
      !ExecSql("INSERT INTO main.file_hashes (file_id, hash) "
               "SELECT fh.file_id, fh.hash FROM old_db.file_hashes fh "
               "JOIN main.files f ON fh.file_id = f.id;")) {
    return AbortMigration();
  }

  std::cout << "[2/4] Migrating valid nodes (pruning empty paths & void "
               "expressions)..."
            << std::endl;
  if (!ExecSql("INSERT OR IGNORE INTO main.nodes (id, kind, name, type_id, "
               "file_id, line, column) "
               "SELECT n.id, n.kind, n.name, n.type_id, n.file_id, n.line, "
               "n.column "
               "FROM old_db.nodes n "
               "JOIN main.files f ON n.file_id = f.id "
               "LEFT JOIN main.types t ON n.type_id = t.id "
               "WHERE NOT (n.kind = 6 AND t.name = 'void');")) {
    return AbortMigration();
  }

  std::cout
      << "[3/4] Migrating valid edges into WITHOUT ROWID clustered table..."
      << std::endl;
  if (!ExecSql("INSERT OR IGNORE INTO main.edges (from_id, to_id, kind, "
               "file_id, line, column) "
               "SELECT e.from_id, e.to_id, e.kind, e.file_id, e.line, e.column "
               "FROM old_db.edges e "
               "JOIN main.files f ON e.file_id = f.id "
               "JOIN main.nodes n1 ON e.from_id = n1.id "
               "JOIN main.nodes n2 ON e.to_id = n2.id;") ||
      // Prune legacy reverse Def->Decl Implicit cycle edges
      !ExecSql("DELETE FROM main.edges WHERE (from_id, to_id, kind, file_id, "
               "line, column) IN ("
               "  SELECT e.from_id, e.to_id, e.kind, e.file_id, e.line, "
               "e.column "
               "  FROM main.edges e "
               "  JOIN main.edges e_opp ON e.from_id = e_opp.to_id AND "
               "e.to_id = e_opp.from_id AND e.kind = 6 AND e_opp.kind = 6 "
               "  JOIN main.nodes n_from ON e.from_id = n_from.id "
               "  WHERE n_from.line = e.line AND n_from.column = e.column"
               ");") ||
      // Prune legacy operator-> and operator* return edges
      !ExecSql("DELETE FROM main.edges WHERE from_id IN ("
               "  SELECT id FROM main.nodes WHERE name IN "
               "('operator->_return', 'operator*_return')"
               ");") ||
      // Bypass legacy IdentityFunction bottlenecks (excluding 'isolate' arg of
      // direct_handle/handle)
      !ExecSql("INSERT OR IGNORE INTO main.edges (from_id, to_id, kind, "
               "file_id, line, column) "
               "SELECT e_arg.from_id, e_ret.to_id, 6, e_ret.file_id, "
               "e_ret.line, e_ret.column "
               "FROM main.edges e_ret "
               "JOIN main.nodes n_ret ON e_ret.from_id = n_ret.id "
               "JOIN main.edges e_arg ON e_arg.file_id = e_ret.file_id AND "
               "e_arg.kind = 3 AND e_arg.line BETWEEN e_ret.line AND "
               "e_ret.line + 2 "
               "JOIN main.nodes n_param ON e_arg.to_id = n_param.id "
               "WHERE e_ret.kind = 4 "
               "  AND n_ret.name IN ('DecompressTagged_return', "
               "'DecompressTaggedSigned_return', 'DecompressTaggedAny_return', "
               "'CompressObject_return', 'CompressAny_return', "
               "'direct_handle_return', 'make_handle_return', 'handle_return') "
               "  AND n_param.file_id = n_ret.file_id "
               "  AND ABS(n_param.line - n_ret.line) <= 5 "
               "  AND n_param.name != 'isolate';") ||
      !ExecSql("DELETE FROM main.edges WHERE from_id IN ("
               "  SELECT id FROM main.nodes WHERE name IN "
               "('DecompressTagged_return', 'DecompressTaggedSigned_return', "
               "'DecompressTaggedAny_return', 'CompressObject_return', "
               "'CompressAny_return', 'direct_handle_return', "
               "'make_handle_return', 'handle_return')"
               ");") ||
      // Prune legacy 0-argument default-constructor CXXConstructExpr
      // initialization edges
      !ExecSql("DELETE FROM main.edges WHERE kind = 2 AND from_id IN ("
               "  SELECT n.id FROM main.nodes n "
               "  LEFT JOIN main.edges e_in ON e_in.to_id = n.id "
               "  WHERE n.name = 'CXXConstructExpr' AND e_in.from_id IS NULL"
               ");") ||
      !ExecSql("COMMIT;") || !ExecSql("DETACH DATABASE old_db;")) {
    return AbortMigration();
  }

  std::cout << "[4/4] Running ANALYZE & WAL checkpoint..." << std::endl;
  ExecSql("ANALYZE;");
  ExecSql("PRAGMA wal_checkpoint(TRUNCATE);");
  sqlite3_close(NewDb);

  auto EndTime = std::chrono::steady_clock::now();
  double TotalElapsed =
      std::chrono::duration_cast<std::chrono::seconds>(EndTime - StartTime)
          .count();
  std::cout << "SUCCESS: Database migration & compaction completed in "
            << static_cast<int>(TotalElapsed / 60) << "m "
            << static_cast<int>(TotalElapsed) % 60 << "s!" << std::endl;
  return true;
}

static void IncreaseFileDescriptorLimit() {
  struct rlimit Lim;
  if (getrlimit(RLIMIT_NOFILE, &Lim) == 0) {
    Lim.rlim_cur = Lim.rlim_max;  // Set soft limit to hard limit
    if (setrlimit(RLIMIT_NOFILE, &Lim) != 0) {
      std::cerr << "[Warning] Failed to increase file descriptor limit. "
                   "Concurrency might be restricted."
                << std::endl;
    }
  }
}

int main(int argc, const char** argv) {
  IncreaseFileDescriptorLimit();

  // Parse LLVM CommandLine options in server, index, or migrate modes
  std::vector<std::string> PositionalFilters;
  if (HasFlag(argc, argv, "--server") || HasFlag(argc, argv, "--index") ||
      HasFlag(argc, argv, "--migrate-db")) {
    cl::list<std::string> IndexFiltersOpt(
        cl::Positional, cl::desc("[<file-filters>...]"), cl::ZeroOrMore,
        cl::cat(DataFlowAnalyzerCategory));
    cl::ParseCommandLineOptions(argc, argv, "C++ Data Flow Analyzer\n");
    PositionalFilters.assign(IndexFiltersOpt.begin(), IndexFiltersOpt.end());

    // Load optional custom wrapper JSON configuration
    if (!WrapperConfigOpt.empty()) {
      std::cerr << "Loading custom wrapper configuration from: "
                << WrapperConfigOpt << std::endl;
      if (!dfg::WrapperConfigManager::Get().LoadFromFile(WrapperConfigOpt)) {
        std::cerr << "ERROR: Failed to load wrapper configuration from: "
                  << WrapperConfigOpt << std::endl;
        return 1;
      }
    }
  }

  // 1. Check Database Migration Mode
  if (MigrateMode) {
    std::string OldDb = OldDbOpt;
    std::string NewDb = NewDbOpt;
    if (OldDb.empty() || NewDb.empty()) {
      std::cerr
          << "ERROR: Please specify both --old-db and --new-db for migration."
          << std::endl;
      return 1;
    }
    if (MigrateDatabase(OldDb, NewDb)) {
      return 0;
    } else {
      return 1;
    }
  }

  // 2. Check Server Mode
  if (ServerMode) {
    dfg::Server Server;
    Server.Run();
    return 0;
  }

  // 3. Check Standalone Index Mode
  if (IndexMode) {
    std::string WorkspaceRoot = WorkspaceRootOpt;
    try {
      WorkspaceRoot = std::filesystem::canonical(WorkspaceRoot).string();
    } catch (...) {
      WorkspaceRoot = std::filesystem::absolute(WorkspaceRoot).string();
    }
    std::string CacheDir = CacheDirOpt;
    std::string CompileCommandsDir = CompileCommandsDirOpt;

    if (CacheDir.empty()) {
      CacheDir = dfg::GetDefaultCacheDir();
    }
    if (CompileCommandsDir.empty()) {
      CompileCommandsDir = WorkspaceRoot + "/build";
    }

    std::cout << "Initializing standalone Indexer..." << std::endl;
    std::cout << "  Workspace: " << WorkspaceRoot << std::endl;
    std::cout << "  Cache: " << CacheDir << std::endl;
    std::cout << "  CompileCommands: " << CompileCommandsDir << std::endl;

    // Create cache directory
    llvm::sys::fs::create_directories(CacheDir);

    // Open Database
    dfg::Database Db;
    std::string DbPath = CacheDir + "/.dataflow.db";
    if (!Db.Open(DbPath)) {
      std::cerr << "ERROR: Failed to open database at " << DbPath << std::endl;
      return 1;
    }

    // Load Compilation Database
    std::string ErrorMsg;
    auto Compilations = clang::tooling::CompilationDatabase::loadFromDirectory(
        CompileCommandsDir, ErrorMsg);
    if (!Compilations) {
      Compilations = clang::tooling::CompilationDatabase::loadFromDirectory(
          WorkspaceRoot, ErrorMsg);
      if (!Compilations) {
        std::cerr << "ERROR: Failed to load compilation database: " << ErrorMsg
                  << std::endl;
        return 1;
      }
    }

    unsigned NumThreads = ConcurrencyOpt;
    dfg::Indexer Indexer(WorkspaceRoot, Db, NumThreads, TelemetryOpt);

    auto StartTime = std::chrono::steady_clock::now();
    auto ProgressCb = [StartTime](unsigned Current, unsigned Total,
                                  const std::string& File) {
      auto Now = std::chrono::steady_clock::now();
      double Elapsed =
          std::chrono::duration_cast<std::chrono::seconds>(Now - StartTime)
              .count();
      std::string RemainingStr = "estimating...";
      if (Current > 0 && Elapsed > 0.5) {
        double Rate = static_cast<double>(Current) / Elapsed;
        if (Rate > 0.01) {
          double RemainingSeconds = (Total - Current) / Rate;
          unsigned RemMin = static_cast<unsigned>(RemainingSeconds) / 60;
          unsigned RemSec = static_cast<unsigned>(RemainingSeconds) % 60;
          RemainingStr =
              std::to_string(RemMin) + "m " + std::to_string(RemSec) + "s";
        }
      }
      std::cout << "[Index Progress] [" << Current << "/" << Total << "] "
                << File << " (" << (Current * 100 / Total)
                << "%, Remaining: " << RemainingStr << ")" << std::endl;
    };

    std::cout << "Starting codebase indexing..." << std::endl;
    if (Indexer.BuildIndex(*Compilations, ProgressCb, PositionalFilters)) {
      int Nodes = 0, Edges = 0;
      Db.GetStats(Nodes, Edges);
      std::cout << "SUCCESS: Indexing complete! Nodes: " << Nodes
                << ", Edges: " << Edges << std::endl;
      return 0;
    } else {
      std::cerr << "ERROR: Indexing failed." << std::endl;
      return 1;
    }
  }

  // 4. Default CLI Mode (Single file parse & JSON dump)
  std::cout << "Starting C++ Data Flow Analyzer (CLI Mode)..." << std::endl;

  auto ExpectedParser =
      CommonOptionsParser::create(argc, argv, DataFlowAnalyzerCategory);
  if (!ExpectedParser) {
    errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser& OptionsParser = ExpectedParser.get();

  // Load optional custom wrapper JSON configuration in default CLI mode
  if (!WrapperConfigOpt.empty()) {
    std::cerr << "Loading custom wrapper configuration from: "
              << WrapperConfigOpt << std::endl;
    if (!dfg::WrapperConfigManager::Get().LoadFromFile(WrapperConfigOpt)) {
      std::cerr << "ERROR: Failed to load wrapper configuration from: "
                << WrapperConfigOpt << std::endl;
      return 1;
    }
  }
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  // Append dynamically resolved Clang resource directory to compilation arguments
  std::string ResourceDir = dfg::GetClangResourceDir();
  std::string ResourceDirArg;
  if (!ResourceDir.empty()) {
    ResourceDirArg = "-resource-dir=" + ResourceDir;
    auto ResourceDirAdjuster = clang::tooling::getInsertArgumentAdjuster(
        ResourceDirArg.c_str(),
        clang::tooling::ArgumentInsertPosition::END);
    Tool.appendArgumentsAdjuster(ResourceDirAdjuster);
  }

  // Open in-memory SQLite DB for CLI mode
  dfg::Database Db;
  if (!Db.Open(":memory:")) {
    std::cerr << "ERROR: Failed to open SQLite database in memory" << std::endl;
    return 1;
  }

  dfg::DfgGraph Graph;
  dfg::PathTranslator Translator("");
  DfgActionFactory Factory(Graph, Translator);
  int Result = Tool.run(&Factory);

  std::cout << "Finished with result: " << Result << std::endl;

  if (Result == 0) {
    Db.BeginTransaction();
    for (const auto& N : Graph.Nodes) {
      Db.InsertNode(N);
    }
    for (const auto& E : Graph.Edges) {
      Db.InsertEdge(E);
    }
    Db.CommitTransaction();
  }

  if (Result == 0) {
    std::vector<dfg::Node> Nodes;
    std::vector<dfg::Edge> Edges;
    if (Db.DumpAll(Nodes, Edges)) {
      llvm::json::Array JsonNodes;
      for (const auto& n : Nodes) {
        llvm::json::Object LocObj{
            {"file", Translator.ToAbsolute(n.Loc.File)},
            {"line", static_cast<int64_t>(n.Loc.Line)},
            {"column", static_cast<int64_t>(n.Loc.Column)},
        };
        llvm::json::Object NodeObj{
            {"id", n.ID},
            {"kind", dfg::NodeKindToString(n.Kind)},
            {"name", n.Name},
            {"type", n.Type},
            {"location", std::move(LocObj)},
        };
        JsonNodes.push_back(std::move(NodeObj));
      }

      llvm::json::Array JsonEdges;
      for (const auto& e : Edges) {
        llvm::json::Object LocObj{
            {"file", Translator.ToAbsolute(e.Loc.File)},
            {"line", static_cast<int64_t>(e.Loc.Line)},
            {"column", static_cast<int64_t>(e.Loc.Column)},
        };
        llvm::json::Object EdgeObj{
            {"from", e.From},
            {"to", e.To},
            {"kind", dfg::EdgeKindToString(e.Kind)},
            {"location", std::move(LocObj)},
        };
        JsonEdges.push_back(std::move(EdgeObj));
      }

      llvm::json::Object Root{
          {"nodes", std::move(JsonNodes)},
          {"edges", std::move(JsonEdges)},
      };

      std::string S;
      llvm::raw_string_ostream OS(S);
      OS << llvm::json::Value(std::move(Root));

      std::cout << "--- DFG JSON ---" << std::endl;
      std::cout << S << std::endl;
      std::cout << "----------------" << std::endl;
    } else {
      std::cerr << "ERROR: Failed to dump database." << std::endl;
    }
  }

  return Result;
}
