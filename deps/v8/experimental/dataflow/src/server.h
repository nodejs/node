// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include <string>

#include "database.h"
#include "index.h"
#include "llvm/Support/JSON.h"
#include "path_translator.h"
#include "query.h"

namespace dfg {

class Server {
 public:
  Server() = default;

  // Start the server loop (reads from stdin, writes to stdout)
  void Run();

 private:
  Database Db;                                 // SQLite Database instance
  std::unique_ptr<PathTranslator> Translator;  // PathTranslator instance
  std::unique_ptr<Indexer> Index;
  std::unique_ptr<QueryEngine> Queries;
  std::string WorkspaceRoot;
  std::string CacheDir;

  // Handle a single JSON-RPC request string and return the response string
  std::string HandleRequest(const std::string& RequestStr);

  // Helper to create JSON-RPC error response
  std::string CreateErrorResponse(llvm::json::Value Id, int Code,
                                  const std::string& Message);

  // Helper to create JSON-RPC success response
  std::string CreateSuccessResponse(llvm::json::Value Id,
                                    llvm::json::Value Result);
};

}  // namespace dfg

#endif  // SERVER_H
