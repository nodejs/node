// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PATH_TRANSLATOR_H
#define PATH_TRANSLATOR_H

#include <filesystem>
#include <mutex>
#include <string>
#include "llvm/ADT/StringMap.h"

namespace dfg {

class PathTranslator {
 public:
  explicit PathTranslator(const std::string& WorkspaceRoot) {
    if (!WorkspaceRoot.empty()) {
      try {
        this->WorkspaceRoot =
            std::filesystem::canonical(WorkspaceRoot).string();
      } catch (...) {
        try {
          this->WorkspaceRoot =
              std::filesystem::weakly_canonical(WorkspaceRoot).string();
        } catch (...) {
          this->WorkspaceRoot = WorkspaceRoot;
        }
      }
    }
    // Ensure WorkspaceRoot does NOT end with a trailing slash for consistent
    // stripping
    if (!this->WorkspaceRoot.empty() && (this->WorkspaceRoot.back() == '/' ||
                                         this->WorkspaceRoot.back() == '\\')) {
      this->WorkspaceRoot.pop_back();
    }
  }
  bool IsEmpty() const { return WorkspaceRoot.empty(); }


  std::string ToRelative(const std::string& AbsPath) const {
    if (WorkspaceRoot.empty()) return AbsPath;

    {
      std::lock_guard<std::mutex> Lock(CacheMutex);
      auto it = RelativeCache.find(AbsPath);
      if (it != RelativeCache.end()) {
        return it->getValue();
      }
    }

    std::string CanonicalAbs = AbsPath;
    if (!AbsPath.empty()) {
      try {
        CanonicalAbs = std::filesystem::canonical(AbsPath).string();
      } catch (...) {
        try {
          CanonicalAbs = std::filesystem::weakly_canonical(AbsPath).string();
        } catch (...) {
        }
      }
    }

    std::string Result = AbsPath;
    if (CanonicalAbs.find(WorkspaceRoot) == 0) {
      std::string Rel = CanonicalAbs.substr(WorkspaceRoot.length());
      if (!Rel.empty() && (Rel[0] == '/' || Rel[0] == '\\')) {
        Rel = Rel.substr(1);
      }
      Result = Rel;
    }

    {
      std::lock_guard<std::mutex> Lock(CacheMutex);
      RelativeCache.try_emplace(AbsPath, Result);
    }
    return Result;
  }

  std::string ToAbsolute(const std::string& RelPath) const {
    if (WorkspaceRoot.empty() || RelPath.empty()) return RelPath;

    // If it's already absolute (POSIX or Windows), don't prepend
    if (RelPath[0] == '/' || (RelPath.length() > 1 && RelPath[1] == ':')) {
      return RelPath;
    }

    return WorkspaceRoot + "/" + RelPath;
  }

 private:
  std::string WorkspaceRoot;
  mutable llvm::StringMap<std::string> RelativeCache;
  mutable std::mutex CacheMutex;
};

}  // namespace dfg

#endif  // PATH_TRANSLATOR_H
