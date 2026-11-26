// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "server.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "clang/Tooling/CompilationDatabase.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

namespace dfg {

void Server::Run() {
  std::cerr << "Data Flow Analyzer Server started. Listening on stdin..."
            << std::endl;
  std::string Line;
  while (std::getline(std::cin, Line)) {
    if (Line.empty()) continue;
    std::string Response = HandleRequest(Line);
    std::cout << Response << std::endl;  // std::endl flushes
  }
}

std::string Server::CreateErrorResponse(llvm::json::Value Id, int Code,
                                        const std::string& Message) {
  llvm::json::Object ErrorObj{
      {"code", Code},
      {"message", Message},
  };
  llvm::json::Object RespObj{
      {"jsonrpc", "2.0"},
      {"id", std::move(Id)},
      {"error", std::move(ErrorObj)},
  };
  std::string S;
  llvm::raw_string_ostream OS(S);
  OS << llvm::json::Value(std::move(RespObj));
  return S;
}

std::string Server::CreateSuccessResponse(llvm::json::Value Id,
                                          llvm::json::Value Result) {
  llvm::json::Object RespObj{
      {"jsonrpc", "2.0"},
      {"id", std::move(Id)},
      {"result", std::move(Result)},
  };
  std::string S;
  llvm::raw_string_ostream OS(S);
  OS << llvm::json::Value(std::move(RespObj));
  return S;
}

std::string Server::HandleRequest(const std::string& RequestStr) {
  auto ExpectedVal = llvm::json::parse(RequestStr);
  if (!ExpectedVal) {
    return CreateErrorResponse(nullptr, -32700, "Parse error");
  }

  auto* ReqObj = ExpectedVal->getAsObject();
  if (!ReqObj) {
    return CreateErrorResponse(nullptr, -32600, "Invalid Request");
  }

  llvm::json::Value Id = nullptr;
  if (auto* IdVal = ReqObj->get("id")) {
    Id = *IdVal;
  }

  std::string Method = ReqObj->getString("method").value_or("").str();
  auto* Params = ReqObj->getObject("params");

  if (Method == "initialize") {
    if (!Params) {
      return CreateErrorResponse(Id, -32602,
                                 "Invalid params (expected object)");
    }

    WorkspaceRoot = Params->getString("workspaceRoot").value_or("").str();
    CacheDir = Params->getString("cacheDir").value_or("").str();
    std::string CompileCommandsDir =
        Params->getString("compileCommandsDir").value_or("").str();

    if (WorkspaceRoot.empty()) {
      return CreateErrorResponse(Id, -32602, "Missing workspaceRoot");
    }
    if (CacheDir.empty()) {
      CacheDir = GetDefaultCacheDir();
    }
    if (CompileCommandsDir.empty()) {
      CompileCommandsDir = WorkspaceRoot + "/build";
    }

    std::cerr << "Initializing server:\n  Workspace: " << WorkspaceRoot
              << "\n  Cache: " << CacheDir
              << "\n  CompileCommands: " << CompileCommandsDir << std::endl;

    // Create cache/db directory if it doesn't exist
    llvm::sys::fs::create_directories(CacheDir);

    // Open SQLite Database
    std::string DbPath = CacheDir + "/.dataflow.db";
    if (!Db.Open(DbPath)) {
      return CreateErrorResponse(Id, -32603,
                                 "Failed to open SQLite database at " + DbPath);
    }

    std::string ErrorMsg;
    auto Compilations = clang::tooling::CompilationDatabase::loadFromDirectory(
        CompileCommandsDir, ErrorMsg);
    if (!Compilations) {
      Compilations = clang::tooling::CompilationDatabase::loadFromDirectory(
          WorkspaceRoot, ErrorMsg);
      if (!Compilations) {
        return CreateErrorResponse(
            Id, -32603, "Failed to load compilation database: " + ErrorMsg);
      }
    }

    int64_t Concurrency = Params->getInteger("concurrency").value_or(0);

    // Instantiate PathTranslator
    Translator = std::make_unique<PathTranslator>(WorkspaceRoot);

    Index = std::make_unique<Indexer>(WorkspaceRoot, Db,
                                      static_cast<unsigned>(Concurrency));
    std::cerr << "Building index..." << std::endl;

    auto StartTime = std::chrono::steady_clock::now();
    auto ProgressCb = [StartTime](unsigned Current, unsigned Total,
                                  const std::string& File) {
      auto Now = std::chrono::steady_clock::now();
      double Elapsed =
          std::chrono::duration_cast<std::chrono::seconds>(Now - StartTime)
              .count();
      std::string MessageStr = "[" + std::to_string(Current) + "/" +
                               std::to_string(Total) + "] " + File;
      if (Current > 0 && Elapsed > 0.5) {
        double Rate = static_cast<double>(Current) / Elapsed;
        if (Rate > 0.01) {
          double RemainingSeconds = (Total - Current) / Rate;
          unsigned RemMin = static_cast<unsigned>(RemainingSeconds) / 60;
          unsigned RemSec = static_cast<unsigned>(RemainingSeconds) % 60;
          MessageStr += " (Remaining: " + std::to_string(RemMin) + "m " +
                        std::to_string(RemSec) + "s)";
        }
      }

      llvm::json::Object ParamsObj{
          {"title", "Indexing C++ files"},
          {"message", MessageStr},
          {"percentage", static_cast<int64_t>(Total > 0 ? (Current * 100) / Total : 0)},
      };
      llvm::json::Object NotificationObj{
          {"jsonrpc", "2.0"},
          {"method", "window/progress"},
          {"params", std::move(ParamsObj)},
      };
      std::string S;
      llvm::raw_string_ostream OS(S);
      OS << llvm::json::Value(std::move(NotificationObj));
      std::cout << S << std::endl;
    };

    std::vector<std::string> Filters;
    if (auto *FiltersVal = Params->getArray("filters")) {
      for (const auto& f : *FiltersVal) {
        if (auto s = f.getAsString()) {
          Filters.push_back(s->str());
        }
      }
    }

    bool SkipIndexing = Params->getBoolean("skipIndexing").value_or(false);
    if (!SkipIndexing) {
      if (!Index->BuildIndex(*Compilations, ProgressCb, Filters)) {
        return CreateErrorResponse(Id, -32603, "Failed to build index");
      }
    } else if (!Db.HasCsrIndex()) {
      Db.BuildAndLoadCsrIndex();
    }

    Queries = std::make_unique<QueryEngine>(Db, *Translator);
    std::cerr << "Index built successfully." << std::endl;

    int NodesCount = 0;
    int EdgesCount = 0;
    Db.GetStats(NodesCount, EdgesCount);

    llvm::json::Object ResultObj{
        {"status", "initialized"},
        {"nodesCount", static_cast<int64_t>(NodesCount)},
        {"edgesCount", static_cast<int64_t>(EdgesCount)},
    };
    return CreateSuccessResponse(Id, std::move(ResultObj));
  }

  if (Method == "query/incoming" || Method == "query/outgoing" || Method == "query/from-leaves") {
    if (!Queries || !Translator) {
      return CreateErrorResponse(Id, -32001, "Server not initialized");
    }
    if (!Params) {
      return CreateErrorResponse(Id, -32602, "Invalid params");
    }

    std::string File = Params->getString("file").value_or("").str();
    unsigned Line = Params->getInteger("line").value_or(0);
    unsigned Column = Params->getInteger("column").value_or(0);

    if (File.empty() || Line == 0 || Column == 0) {
      return CreateErrorResponse(Id, -32602, "Invalid file, line, or column");
    }

    std::shared_ptr<QueryResultNode> Result;
    if (Method == "query/incoming") {
      Result = Queries->QueryIncoming(File, Line, Column);
    } else if (Method == "query/outgoing") {
      Result = Queries->QueryOutgoing(File, Line, Column);
    } else {
      Result = Queries->QueryFromLeaves(File, Line, Column);
    }

    if (!Result) {
      return CreateSuccessResponse(Id, nullptr);
    }

    return CreateSuccessResponse(Id, Result->ToJson(*Translator));
  }

  return CreateErrorResponse(Id, -32601, "Method not found: " + Method);
}

}  // namespace dfg
