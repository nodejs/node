// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/base/file_util.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include "experimental/redline/base/log.h"

namespace redline {

bool ReadFileToString(const std::string& path, std::string* contents,
                      std::string* error) {
  FILE* file = fopen(path.c_str(), "rbe");
  if (file == nullptr) {
    *error = StringPrintf("Cannot open %s: %s", path.c_str(), strerror(errno));
    return false;
  }
  contents->clear();
  char buffer[64 * 1024];
  while (true) {
    size_t n = fread(buffer, 1, sizeof(buffer), file);
    if (n > 0) {
      contents->append(buffer, n);
    }
    if (n < sizeof(buffer)) {
      break;
    }
  }
  bool ok = ferror(file) == 0;
  if (!ok) {
    *error =
        StringPrintf("Error reading %s: %s", path.c_str(), strerror(errno));
  }
  fclose(file);
  return ok;
}

bool WriteStringToFile(const std::string& path, std::string_view contents,
                       std::string* error) {
  FILE* file = fopen(path.c_str(), "wbe");
  if (file == nullptr) {
    *error =
        StringPrintf("Cannot create %s: %s", path.c_str(), strerror(errno));
    return false;
  }
  bool ok =
      fwrite(contents.data(), 1, contents.size(), file) == contents.size();
  if (fclose(file) != 0) {
    ok = false;
  }
  if (!ok) {
    *error =
        StringPrintf("Error writing %s: %s", path.c_str(), strerror(errno));
  }
  return ok;
}

bool MakeDirectories(const std::string& path, std::string* error) {
  if (path.empty()) {
    return true;
  }
  std::string current;
  size_t start = 0;
  while (start <= path.size()) {
    size_t end = path.find('/', start);
    if (end == std::string::npos) {
      end = path.size();
    }
    current = path.substr(0, end);
    start = end + 1;
    if (current.empty()) {
      continue;
    }
    if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
      *error = StringPrintf("Cannot create directory %s: %s", current.c_str(),
                            strerror(errno));
      return false;
    }
  }
  if (!IsDirectory(path)) {
    *error = StringPrintf("%s exists but is not a directory", path.c_str());
    return false;
  }
  return true;
}

bool IsDirectory(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool FileExists(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

namespace {

bool EndsWith(const std::string& text, const std::string& suffix) {
  return text.ends_with(suffix);
}

void ListFilesInto(const std::string& directory, const std::string& suffix,
                   std::vector<std::string>* out) {
  DIR* dir = opendir(directory.c_str());
  if (dir == nullptr) {
    return;
  }
  while (struct dirent* entry = readdir(dir)) {
    std::string name = entry->d_name;
    if (name == "." || name == "..") {
      continue;
    }
    std::string path = directory;
    path += '/';
    path += name;
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      ListFilesInto(path, suffix, out);
    } else if (S_ISREG(st.st_mode) && EndsWith(name, suffix)) {
      out->push_back(path);
    }
  }
  closedir(dir);
}

}  // namespace

std::vector<std::string> ListFiles(const std::string& directory,
                                   const std::string& suffix) {
  std::vector<std::string> files;
  std::string root = directory;
  while (root.size() > 1 && root.back() == '/') {
    root.pop_back();
  }
  struct stat st;
  if (stat(root.c_str(), &st) != 0) {
    return files;
  }
  if (S_ISREG(st.st_mode)) {
    if (EndsWith(root, suffix)) {
      files.push_back(root);
    }
    return files;
  }
  ListFilesInto(root, suffix, &files);
  std::sort(files.begin(), files.end());
  return files;
}

std::string BaseName(const std::string& path) {
  size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) {
    return path;
  }
  return path.substr(slash + 1);
}

std::string RedlineDataDir(const std::string& name) {
  char buffer[4096];
  ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (n <= 0) {
    return "";
  }
  std::string exe(buffer, static_cast<size_t>(n));
  size_t slash = exe.rfind('/');
  if (slash == std::string::npos) {
    return "";
  }
  for (const char* relative : {"/", "/../../experimental/redline/"}) {
    std::string candidate = exe.substr(0, slash);
    candidate += relative;
    candidate += name;
    if (IsDirectory(candidate)) {
      return candidate;
    }
  }
  return "";
}

}  // namespace redline
