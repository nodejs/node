// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "index.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "clang/Tooling/ArgumentsAdjusters.h"
#include "clang/Tooling/Tooling.h"
#include "dfg_visitor.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MD5.h"
#include "llvm/Support/MemoryBuffer.h"
#include "path_translator.h"

namespace dfg {

static std::string CalculateFileHash(const std::string& FilePath) {
  auto BufferOrErr = llvm::MemoryBuffer::getFile(FilePath);
  if (!BufferOrErr) return "";

  llvm::MD5 Hash;
  Hash.update((*BufferOrErr)->getBuffer());
  llvm::MD5::MD5Result Result;
  Hash.final(Result);
  llvm::SmallString<32> ResultStr;
  llvm::MD5::stringifyResult(Result, ResultStr);
  return ResultStr.str().str();
}

static std::string FindHighestClangVersionDir(const std::string& ParentPath) {
  if (!std::filesystem::exists(ParentPath)) return "";

  std::vector<std::string> Versions;
  try {
    for (const auto& Entry : std::filesystem::directory_iterator(ParentPath)) {
      if (Entry.is_directory()) {
        std::filesystem::path IncludeDir = Entry.path() / "include";
        if (std::filesystem::exists(IncludeDir / "stddef.h")) {
          Versions.push_back(Entry.path().filename().string());
        }
      }
    }
  } catch (...) {
    return "";
  }

  if (Versions.empty()) return "";

  std::sort(Versions.begin(), Versions.end(),
            [](const std::string& a, const std::string& b) {
              std::vector<int> AParts, BParts;
              std::stringstream ssA(a), ssB(b);
              std::string Token;
              while (std::getline(ssA, Token, '.')) {
                try {
                  AParts.push_back(std::stoi(Token));
                } catch (...) {
                  AParts.push_back(0);
                }
              }
              while (std::getline(ssB, Token, '.')) {
                try {
                  BParts.push_back(std::stoi(Token));
                } catch (...) {
                  BParts.push_back(0);
                }
              }
              for (size_t i = 0; i < std::max(AParts.size(), BParts.size());
                   ++i) {
                int valA = i < AParts.size() ? AParts[i] : 0;
                int valB = i < BParts.size() ? BParts[i] : 0;
                if (valA != valB) return valA < valB;
              }
              return false;
            });

  return (std::filesystem::path(ParentPath) / Versions.back()).string();
}

// Dynamically locates Clang's resource headers directory relative to the
// indexer binary path, supporting any installed clang version dynamically
std::string GetClangResourceDir() {
  try {
    std::string BinaryPath =
        std::filesystem::canonical("/proc/self/exe").string();
    std::filesystem::path Parent =
        std::filesystem::path(BinaryPath).parent_path();

    std::filesystem::path LocalClangLib = Parent / "lib" / "clang";
    std::string Found = FindHighestClangVersionDir(LocalClangLib.string());
    if (!Found.empty()) return Found;

    std::filesystem::path ProjectClangLib =
        Parent.parent_path() / "lib" / "clang";
    Found = FindHighestClangVersionDir(ProjectClangLib.string());
    if (!Found.empty()) return Found;

  } catch (...) {
  }

  // Portable fallback to standard clang system installations
  for (const char* Path : {"/usr/lib/clang", "/usr/local/lib/clang"}) {
    std::string Found = FindHighestClangVersionDir(Path);
    if (!Found.empty()) return Found;
  }

  return "";  // Let clang driver locate fallback
}

Indexer::Indexer(const std::string& WorkspaceRoot, Database& Db,
                 unsigned NumThreads, bool EnableTelemetry)
    : WorkspaceRoot(WorkspaceRoot),
      Db(Db),
      NumThreads(NumThreads),
      EnableTelemetry(EnableTelemetry) {}

bool Indexer::BuildIndex(clang::tooling::CompilationDatabase& Compilations,
                         ProgressCallback Callback,
                         const std::vector<std::string>& Filters) {
  auto StartTime = std::chrono::high_resolution_clock::now();
  std::vector<std::string> AllFiles = Compilations.getAllFiles();

  const bool IsV8 = llvm::sys::fs::exists(WorkspaceRoot + "/include/v8.h");
  std::string BuildDir = "";
  auto Commands = Compilations.getAllCompileCommands();
  if (!Commands.empty()) {
    BuildDir = Commands[0].Directory;
  }

  std::vector<std::string> CoreFiles;
  for (const auto& File : AllFiles) {
    if (!Filters.empty()) {
      bool Matched = false;
      // Normalize file path to POSIX and strip leading relative prefixes
      std::string NormFile = File;
      std::replace(NormFile.begin(), NormFile.end(), '\\', '/');
      while (NormFile.rfind("../", 0) == 0) {
        NormFile.erase(0, 3);
      }
      while (NormFile.rfind("./", 0) == 0) {
        NormFile.erase(0, 2);
      }

      for (const auto& Filter : Filters) {
        // Normalize filter path to POSIX and strip leading relative prefixes
        std::string NormFilter = Filter;
        std::replace(NormFilter.begin(), NormFilter.end(), '\\', '/');
        while (NormFilter.rfind("../", 0) == 0) {
          NormFilter.erase(0, 3);
        }
        while (NormFilter.rfind("./", 0) == 0) {
          NormFilter.erase(0, 2);
        }

        // Exact match or suffix matching (checks if NormFile ends with
        // NormFilter)
        if (NormFile == NormFilter ||
            (NormFile.length() > NormFilter.length() &&
             NormFile.compare(NormFile.length() - NormFilter.length(),
                              NormFilter.length(), NormFilter) == 0 &&
             NormFile[NormFile.length() - NormFilter.length() - 1] == '/')) {
          Matched = true;
          break;
        }
      }
      if (Matched) {
        CoreFiles.push_back(File);
      }
    } else {
      if (IsV8) {
        if ((File.find("/src/") != std::string::npos ||
             File.find("/torque-generated/src/") != std::string::npos) &&
            File.find("/third_party/") == std::string::npos &&
            File.find("/test/") == std::string::npos &&
            File.find("/testing/") == std::string::npos &&
            File.find("check-header-includes") == std::string::npos &&
            File.find("/arm/") == std::string::npos &&
            File.find("/arm64/") == std::string::npos &&
            File.find("/ia32/") == std::string::npos &&
            File.find("/ppc64/") == std::string::npos &&
            File.find("/s390/") == std::string::npos &&
            File.find("/riscv/") == std::string::npos &&
            File.find("/loong64/") == std::string::npos &&
            File.find("/mips64/") == std::string::npos) {
          if (!BuildDir.empty()) {
            auto FileCmds = Compilations.getCompileCommands(File);
            if (!FileCmds.empty() && FileCmds[0].Directory != BuildDir) {
              continue;
            }
          }
          CoreFiles.push_back(File);
        }
      } else {
        CoreFiles.push_back(File);
      }
    }
  }
  AllFiles = std::move(CoreFiles);
  std::vector<std::string> FilesToReindex;

  unsigned Total = AllFiles.size();
  unsigned CacheHitsCount = 0;

  std::cerr << "[Indexer] Total files in compilation database: " << Total
            << std::endl;

  PathTranslator Translator(WorkspaceRoot);

  // Check Cache Hits/Misses (relative paths resolve correctly relative to
  // BuildDir)
  Db.SetEnableTelemetry(EnableTelemetry);
  std::vector<std::string> StaleRelFiles;
  for (const auto& SourceFile : AllFiles) {
    std::filesystem::path AbsPath = std::filesystem::path(SourceFile);
    if (AbsPath.is_relative() && !BuildDir.empty()) {
      AbsPath = std::filesystem::weakly_canonical(
          std::filesystem::path(BuildDir) / AbsPath);
    } else {
      AbsPath = std::filesystem::weakly_canonical(AbsPath);
    }
    std::string AbsSourceFile = AbsPath.string();

    std::string RelFile = Translator.ToRelative(AbsSourceFile);
    std::string CurrentHash = CalculateFileHash(AbsSourceFile);
    std::string CachedHash = Db.GetFileHash(RelFile);

    bool CacheValid = false;

    if ((!CurrentHash.empty() && CachedHash == CurrentHash) ||
        (CurrentHash.empty() && !CachedHash.empty())) {
      std::cerr << "[Indexer] Cache hit for " << SourceFile << std::endl;
      CacheValid = true;
      CacheHitsCount++;

      if (Callback) {
        Callback(CacheHitsCount, Total, SourceFile);
      }
    }

    if (!CacheValid && !CurrentHash.empty()) {
      std::cerr << "[Indexer] Cache miss/invalid for " << SourceFile
                << std::endl;
      StaleRelFiles.push_back(RelFile);
      FilesToReindex.push_back(AbsSourceFile);
    }
  }
  if (!StaleRelFiles.empty()) {
    Db.BeginTransaction();
    Db.DeleteFilesData(StaleRelFiles);
    Db.CommitTransaction();
  }

  if (!FilesToReindex.empty()) {
    unsigned ActualThreads = NumThreads;
    if (ActualThreads == 0) {
      unsigned Cores = std::thread::hardware_concurrency();
      if (Cores == 0) Cores = 4;
      ActualThreads = std::min(Cores, 16u);
    }

    if (FilesToReindex.size() < ActualThreads) {
      ActualThreads = FilesToReindex.size();
    }

    // Dynamic resource-dir and warning adjustments
    std::string ResourceDir = GetClangResourceDir();
    std::string ResourceDirArg = "-resource-dir=" + ResourceDir;
    std::cerr << "[Indexer] Using absolute Clang resource directory: "
              << ResourceDir << std::endl;

    auto ResourceDirAdjuster = clang::tooling::getInsertArgumentAdjuster(
        ResourceDirArg.c_str(),
        clang::tooling::ArgumentInsertPosition::END);

    auto WarningStripper = [](const clang::tooling::CommandLineArguments& Args,
                              llvm::StringRef Filename) {
      clang::tooling::CommandLineArguments AdjustedArgs;
      for (const auto& Arg : Args) {
        if (Arg.find("-Werror") == 0 || Arg.find("-Wfatal-errors") == 0) {
          continue;
        }
        AdjustedArgs.push_back(Arg);
      }
      AdjustedArgs.push_back("-w");
      AdjustedArgs.push_back("-Wno-error");
      return AdjustedArgs;
    };

    // 1. Unified Pipeline Producer-Consumer Model
    ConcurrentQueue<std::unique_ptr<DfgGraph>> DfgQueue(32);
    std::atomic<unsigned> Current{CacheHitsCount};
    std::mutex CallbackMutex;

    auto ToolCallback = [Callback, Total, &Current, &CallbackMutex](const std::string& File) {
      unsigned Curr = ++Current;
      if (Callback) {
        std::lock_guard<std::mutex> Lock(CallbackMutex);
        Callback(Curr, Total, File);
      }
    };

    unsigned TotalGraphs = FilesToReindex.size();
    ConcurrentClaimSet SharedClaims;

    // Spawn the dedicated Database Writer Thread (Consumer) with batched commits
    std::thread DbWriterThread([this, TotalGraphs, &DfgQueue]() {
      auto ThreadStart = std::chrono::high_resolution_clock::now();
      std::unique_ptr<DfgGraph> Graph;
      double CumulativeWriteTime = 0;
      unsigned TotalNodesWritten = 0;
      unsigned TotalEdgesWritten = 0;
      unsigned GraphsWritten = 0;
      llvm::DenseSet<Node::Id> WrittenNodeIds;
      llvm::DenseSet<uint64_t> WrittenEdgeKeys;

      while (DfgQueue.Pop(Graph)) {
        auto WriteStart = std::chrono::high_resolution_clock::now();
        Db.BeginTransaction();
        unsigned BatchCount = 0;
        do {
          if (Graph) {
            for (const auto& N : Graph->Nodes) {
              if (WrittenNodeIds.insert(N.ID).second) {
                Db.InsertNode(N);
                TotalNodesWritten++;
              }
            }
            for (const auto& E : Graph->Edges) {
              uint64_t EdgeKey =
                  E.From ^ (E.To * 0x9ddfea08eb382d69ULL) ^
                  (static_cast<uint64_t>(E.Kind) * 0x517cc1b727220a95ULL);
              if (WrittenEdgeKeys.insert(EdgeKey).second) {
                Db.InsertEdge(E);
                TotalEdgesWritten++;
              }
            }
            if (!Graph->FilePath.empty() && !Graph->FileHash.empty()) {
              Db.UpdateFileHash(Graph->FilePath, Graph->FileHash);
            }
            GraphsWritten++;
            BatchCount++;
          }
        } while (BatchCount < 32 && DfgQueue.TryPop(Graph));
        Db.CommitTransaction();
        auto WriteEnd = std::chrono::high_resolution_clock::now();
        CumulativeWriteTime += std::chrono::duration<double>(WriteEnd - WriteStart).count();

        if ((GraphsWritten / 10) > ((GraphsWritten - BatchCount) / 10) ||
            GraphsWritten == TotalGraphs) {
          std::cerr << "[DB Writer Progress] Flushed " << GraphsWritten << "/"
                    << TotalGraphs << " AST graphs to SQLite..." << std::endl;
        }
      }

      auto ThreadEnd = std::chrono::high_resolution_clock::now();
      double ThreadDuration = std::chrono::duration<double>(ThreadEnd - ThreadStart).count();
      if (EnableTelemetry) {
        std::cerr << "[Telemetry] [DbWriterThread] Finished. "
                  << "Total: " << ThreadDuration << "s, "
                  << "DB Write Time: " << CumulativeWriteTime << "s, "
                  << "Nodes Written: " << TotalNodesWritten << ", "
                  << "Edges Written: " << TotalEdgesWritten << std::endl;
      }
    });

    // Spawn the Parallel Indexing Threads (Producers)
    std::atomic<size_t> NextFileIndex{0};
    std::vector<std::thread> Producers;

    struct DfgActionFactory : public clang::tooling::FrontendActionFactory {
      DfgActionFactory(DfgGraph& Graph, const PathTranslator& Translator,
                       std::function<void(const std::string&)> FileCallback,
                       ConcurrentClaimSet* SharedClaims)
          : Graph(Graph),
            Translator(Translator),
            FileCallback(FileCallback),
            SharedClaims(SharedClaims) {}

      std::unique_ptr<clang::FrontendAction> create() override {
        return std::make_unique<DfgAction>(Graph, Translator, FileCallback,
                                           SharedClaims);
      }

      DfgGraph& Graph;
      const PathTranslator& Translator;
      std::function<void(const std::string&)> FileCallback;
      ConcurrentClaimSet* SharedClaims;
    };

    for (unsigned i = 0; i < ActualThreads; ++i) {
      Producers.push_back(std::thread([this, &Compilations, &FilesToReindex,
                                       &NextFileIndex, &DfgQueue, &SharedClaims,
                                       i, ResourceDirAdjuster, WarningStripper,
                                       ToolCallback]() {
        auto ThreadStart = std::chrono::high_resolution_clock::now();
        double CumulativeToolRunTime = 0;
        std::cerr << "[Thread " << i << "] Started." << std::endl;

        PathTranslator ThreadTranslator(WorkspaceRoot);

        while (true) {
          size_t idx = NextFileIndex.fetch_add(1);
          if (idx >= FilesToReindex.size()) break;

          std::string SourceFile = FilesToReindex[idx];
          std::vector<std::string> SingleFile = {SourceFile};

          clang::tooling::ClangTool Tool(
              Compilations, SingleFile,
              std::make_shared<clang::PCHContainerOperations>(),
              llvm::vfs::createPhysicalFileSystem());
          Tool.appendArgumentsAdjuster(ResourceDirAdjuster);
          Tool.appendArgumentsAdjuster(WarningStripper);

          // Allocate in-memory graph for this file
          auto Graph = std::make_unique<DfgGraph>();
          DfgActionFactory Factory(*Graph, ThreadTranslator, ToolCallback,
                                   &SharedClaims);

          auto ToolStart = std::chrono::high_resolution_clock::now();
          int Result = Tool.run(&Factory);
          auto ToolEnd = std::chrono::high_resolution_clock::now();
          CumulativeToolRunTime += std::chrono::duration<double>(ToolEnd - ToolStart).count();

          if (Result == 0 || !Graph->Nodes.empty()) {
            Graph->FilePath = ThreadTranslator.ToRelative(SourceFile);
            Graph->FileHash = CalculateFileHash(SourceFile);
            DfgQueue.Push(std::move(Graph));
          } else {
            std::cerr << "[Thread " << i
                      << "] WARNING: Failed to parse file: " << SourceFile
                      << " (exit code " << Result << ")" << std::endl;
          }
        }

        auto ThreadEnd = std::chrono::high_resolution_clock::now();
        double ThreadDuration = std::chrono::duration<double>(ThreadEnd - ThreadStart).count();
        if (EnableTelemetry) {
          std::cerr << "[Telemetry] [Thread " << i << "] Finished. "
                    << "Total: " << ThreadDuration << "s, "
                    << "ClangTool Time: " << CumulativeToolRunTime << "s" << std::endl;
        }
      }));
    }

    // Wait for all producers to finish
    for (auto& T : Producers) {
      T.join();
    }

    // Signal the database writer to shutdown (it will exit once the queue is empty)
    DfgQueue.SignalShutdown();

    // Wait for the database writer to finish flushing and join
    DbWriterThread.join();

    auto CsrStart = std::chrono::high_resolution_clock::now();
    if (Db.BuildAndLoadCsrIndex() && EnableTelemetry) {
      auto CsrEnd = std::chrono::high_resolution_clock::now();
      std::cerr << "[Telemetry] [CsrIndex] Built and mmap-loaded CSR index in "
                << std::chrono::duration<double>(CsrEnd - CsrStart).count()
                << "s" << std::endl;
    }

  } else {
    std::cerr << "[Indexer] All files are up to date." << std::endl;
    if (!Db.HasCsrIndex()) {
      auto CsrStart = std::chrono::high_resolution_clock::now();
      if (Db.BuildAndLoadCsrIndex() && EnableTelemetry) {
        auto CsrEnd = std::chrono::high_resolution_clock::now();
        std::cerr
            << "[Telemetry] [CsrIndex] Built and mmap-loaded CSR index in "
            << std::chrono::duration<double>(CsrEnd - CsrStart).count() << "s"
            << std::endl;
      }
    }
  }

  auto OverallEnd = std::chrono::high_resolution_clock::now();
  double OverallDuration =
      std::chrono::duration<double>(OverallEnd - StartTime).count();
  if (EnableTelemetry) {
    std::cerr << "[Telemetry] [Overall] Indexing completed in "
              << OverallDuration << "s" << std::endl;
  }

  return true;
}

}  // namespace dfg
