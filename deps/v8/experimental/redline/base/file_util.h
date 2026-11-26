// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_BASE_FILE_UTIL_H_
#define V8_EXPERIMENTAL_REDLINE_BASE_FILE_UTIL_H_

#include <string>
#include <string_view>
#include <vector>

namespace redline {

// Reads a whole file. Returns false with a message if it cannot be read.
bool ReadFileToString(const std::string& path, std::string* contents,
                      std::string* error);

// Writes `contents` to `path`, replacing any existing file.
bool WriteStringToFile(const std::string& path, std::string_view contents,
                       std::string* error);

// Creates `path` and all missing parents. Succeeds if it already exists.
bool MakeDirectories(const std::string& path, std::string* error);

bool IsDirectory(const std::string& path);
bool FileExists(const std::string& path);
// Stores the byte size of regular file `path` in `size`. Returns false on
// error.
bool FileSize(const std::string& path, size_t* size);

// Lists regular files below `directory` (recursively, following symlinks)
// whose names end with `suffix`, sorted by path. If `directory` is a matching
// file, `ListFiles` returns just that file. If `directory` contains glob
// wildcards (`*`, `?`, `[`), `ListFiles` expands the pattern (`**` matches
// zero or more directory levels). When `suffix` is `".js"`, `ListFiles` skips
// Test262 `*_FIXTURE.js` helper files.
std::vector<std::string> ListFiles(const std::string& directory,
                                   const std::string& suffix);

// Last path component.
std::string BaseName(const std::string& path);

// A data directory of redline (`testdata`, `presets`) found from the location
// of the binary: next to it (a deployed bundle), or in the source tree
// (out/<dir>/redline -> ../../experimental/redline/<name>). Empty if neither
// exists.
std::string RedlineDataDir(const std::string& name);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_BASE_FILE_UTIL_H_
