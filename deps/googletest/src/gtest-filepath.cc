// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "gtest/internal/gtest-filepath.h"

#include <stdlib.h>

#include <iterator>
#include <string>

#include "gtest/gtest-message.h"
#include "gtest/internal/gtest-port.h"

#ifdef GTEST_OS_WINDOWS_MOBILE
#include <windows.h>
#elif defined(GTEST_OS_WINDOWS)
#include <direct.h>
#include <io.h>
#else
#include <limits.h>

#include <climits>  // Some Linux distributions define PATH_MAX here.
#endif              // GTEST_OS_WINDOWS_MOBILE

#include "gtest/internal/gtest-string.h"

#ifdef GTEST_OS_WINDOWS
#define GTEST_PATH_MAX_ _MAX_PATH
#elif defined(PATH_MAX)
#define GTEST_PATH_MAX_ PATH_MAX
#elif defined(_XOPEN_PATH_MAX)
#define GTEST_PATH_MAX_ _XOPEN_PATH_MAX
#else
#define GTEST_PATH_MAX_ _POSIX_PATH_MAX
#endif  // GTEST_OS_WINDOWS

#if GTEST_HAS_FILE_SYSTEM

namespace testing {
namespace internal {

#ifdef GTEST_OS_WINDOWS
// On Windows, '\\' is the standard path separator, but many tools and the
// Windows API also accept '/' as an alternate path separator. Unless otherwise
// noted, a file path can contain either kind of path separators, or a mixture
// of them.
const char kPathSeparator = '\\';
const char kAlternatePathSeparator = '/';
const char kAlternatePathSeparatorString[] = "/";
#ifdef GTEST_OS_WINDOWS_MOBILE
// Windows CE doesn't have a current directory. You should not use
// the current directory in tests on Windows CE, but this at least
// provides a reasonable fallback.
const char kCurrentDirectoryString[] = "\\";
// Windows CE doesn't define INVALID_FILE_ATTRIBUTES
const DWORD kInvalidFileAttributes = 0xffffffff;
#else
const char kCurrentDirectoryString[] = ".\\";
#endif  // GTEST_OS_WINDOWS_MOBILE
#else
const char kPathSeparator = '/';
const char kCurrentDirectoryString[] = "./";
#endif  // GTEST_OS_WINDOWS

// Returns whether the given character is a valid path separator.
static bool IsPathSeparator(char c) {
#if GTEST_HAS_ALT_PATH_SEP_
  return (c == kPathSeparator) || (c == kAlternatePathSeparator);
#else
  return c == kPathSeparator;
#endif
}

// Returns the current working directory, or "" if unsuccessful.
FilePath FilePath::GetCurrentDir() {
#if defined(GTEST_OS_WINDOWS_MOBILE) || defined(GTEST_OS_WINDOWS_PHONE) || \
    defined(GTEST_OS_WINDOWS_RT) || defined(GTEST_OS_ESP8266) ||           \
    defined(GTEST_OS_ESP32) || defined(GTEST_OS_XTENSA) ||                 \
    defined(GTEST_OS_QURT) || defined(GTEST_OS_NXP_QN9090) ||              \
    defined(GTEST_OS_NRF52)
  // These platforms do not have a current directory, so we just return
  // something reasonable.
  return FilePath(kCurrentDirectoryString);
#elif defined(GTEST_OS_WINDOWS)
  char cwd[GTEST_PATH_MAX_ + 1] = {'\0'};
  return FilePath(_getcwd(cwd, sizeof(cwd)) == nullptr ? "" : cwd);
#else
  char cwd[GTEST_PATH_MAX_ + 1] = {'\0'};
  char* result = getcwd(cwd, sizeof(cwd));
#ifdef GTEST_OS_NACL
  // getcwd will likely fail in NaCl due to the sandbox, so return something
  // reasonable. The user may have provided a shim implementation for getcwd,
  // however, so fallback only when failure is detected.
  return FilePath(result == nullptr ? kCurrentDirectoryString : cwd);
#endif  // GTEST_OS_NACL
  return FilePath(result == nullptr ? "" : cwd);
#endif  // GTEST_OS_WINDOWS_MOBILE
}

// Returns a copy of the FilePath with the case-insensitive extension removed.
// Example: FilePath("dir/file.exe").RemoveExtension("EXE") returns
// FilePath("dir/file"). If a case-insensitive extension is not
// found, returns a copy of the original FilePath.
FilePath FilePath::RemoveExtension(const char* extension) const {
  const std::string dot_extension = std::string(".") + extension;
  if (String::EndsWithCaseInsensitive(pathname_, dot_extension)) {
    return FilePath(
        pathname_.substr(0, pathname_.length() - dot_extension.length()));
  }
  return *this;
}

// Returns a pointer to the last occurrence of a valid path separator in
// the FilePath. On Windows, for example, both '/' and '\' are valid path
// separators. Returns NULL if no path separator was found.
const char* FilePath::FindLastPathSeparator() const {
  const char* const last_sep = strrchr(c_str(), kPathSeparator);
#if GTEST_HAS_ALT_PATH_SEP_
  const char* const last_alt_sep = strrchr(c_str(), kAlternatePathSeparator);
  // Comparing two pointers of which only one is NULL is undefined.
  if (last_alt_sep != nullptr &&
      (last_sep == nullptr || last_alt_sep > last_sep)) {
    return last_alt_sep;
  }
#endif
  return last_sep;
}

size_t FilePath::CalculateRootLength() const {
  const auto& path = pathname_;
  auto s = path.begin();
  auto end = path.end();
#ifdef GTEST_OS_WINDOWS
  if (end - s >= 2 && s[1] == ':' && (end - s == 2 || IsPathSeparator(s[2])) &&
      (('A' <= s[0] && s[0] <= 'Z') || ('a' <= s[0] && s[0] <= 'z'))) {
    // A typical absolute path like "C:\Windows" or "D:"
    s += 2;
    if (s != end) {
      ++s;
    }
  } else if (end - s >= 3 && IsPathSeparator(*s) && IsPathSeparator(*(s + 1)) &&
             !IsPathSeparator(*(s + 2))) {
    // Move past the "\\" prefix in a UNC path like "\\Server\Share\Folder"
    s += 2;
    // Skip 2 components and their following separators ("Server\" and "Share\")
    for (int i = 0; i < 2; ++i) {
      while (s != end) {
        bool stop = IsPathSeparator(*s);
        ++s;
        if (stop) {
          break;
        }
      }
    }
  } else if (s != end && IsPathSeparator(*s)) {
    // A drive-rooted path like "\Windows"
    ++s;
  }
#else
  if (s != end && IsPathSeparator(*s)) {
    ++s;
  }
#endif
  return static_cast<size_t>(s - path.begin());
}

// Returns a copy of the FilePath with the directory part removed.
// Example: FilePath("path/to/file").RemoveDirectoryName() returns
// FilePath("file"). If there is no directory part ("just_a_file"), it returns
// the FilePath unmodified. If there is no file part ("just_a_dir/") it
// returns an empty FilePath ("").
// On Windows platform, '\' is the path separator, otherwise it is '/'.
FilePath FilePath::RemoveDirectoryName() const {
  const char* const last_sep = FindLastPathSeparator();
  return last_sep ? FilePath(last_sep + 1) : *this;
}

// RemoveFileName returns the directory path with the filename removed.
// Example: FilePath("path/to/file").RemoveFileName() returns "path/to/".
// If the FilePath is "a_file" or "/a_file", RemoveFileName returns
// FilePath("./") or, on Windows, FilePath(".\\"). If the filepath does
// not have a file, like "just/a/dir/", it returns the FilePath unmodified.
// On Windows platform, '\' is the path separator, otherwise it is '/'.
FilePath FilePath::RemoveFileName() const {
  const char* const last_sep = FindLastPathSeparator();
  std::string dir;
  if (last_sep) {
    dir = std::string(c_str(), static_cast<size_t>(last_sep + 1 - c_str()));
  } else {
    dir = kCurrentDirectoryString;
  }
  return FilePath(dir);
}

// Helper functions for naming files in a directory for xml output.

// Given directory = "dir", base_name = "test", number = 0,
// extension = "xml", returns "dir/test.xml". If number is greater
// than zero (e.g., 12), returns "dir/test_12.xml".
// On Windows platform, uses \ as the separator rather than /.
FilePath FilePath::MakeFileName(const FilePath& directory,
                                const FilePath& base_name, int number,
                                const char* extension) {
  std::string file;
  if (number == 0) {
    file = base_name.string() + "." + extension;
  } else {
    file =
        base_name.string() + "_" + StreamableToString(number) + "." + extension;
  }
  return ConcatPaths(directory, FilePath(file));
}

// Given directory = "dir", relative_path = "test.xml", returns "dir/test.xml".
// On Windows, uses \ as the separator rather than /.
FilePath FilePath::ConcatPaths(const FilePath& directory,
                               const FilePath& relative_path) {
  if (directory.IsEmpty()) return relative_path;
  const FilePath dir(directory.RemoveTrailingPathSeparator());
  return FilePath(dir.string() + kPathSeparator + relative_path.string());
}

// Returns true if pathname describes something findable in the file-system,
// either a file, directory, or whatever.
bool FilePath::FileOrDirectoryExists() const {
#ifdef GTEST_OS_WINDOWS_MOBILE
  LPCWSTR unicode = String::AnsiToUtf16(pathname_.c_str());
  const DWORD attributes = GetFileAttributes(unicode);
  delete[] unicode;
  return attributes != kInvalidFileAttributes;
#else
  posix::StatStruct file_stat{};
  return posix::Stat(pathname_.c_str(), &file_stat) == 0;
#endif  // GTEST_OS_WINDOWS_MOBILE
}

// Returns true if pathname describes a directory in the file-system
// that exists.
bool FilePath::DirectoryExists() const {
  bool result = false;
#ifdef GTEST_OS_WINDOWS
  // Don't strip off trailing separator if path is a root directory on
  // Windows (like "C:\\").
  const FilePath& path(IsRootDirectory() ? *this
                                         : RemoveTrailingPathSeparator());
#else
  const FilePath& path(*this);
#endif

#ifdef GTEST_OS_WINDOWS_MOBILE
  LPCWSTR unicode = String::AnsiToUtf16(path.c_str());
  const DWORD attributes = GetFileAttributes(unicode);
  delete[] unicode;
  if ((attributes != kInvalidFileAttributes) &&
      (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
    result = true;
  }
#else
  posix::StatStruct file_stat{};
  result =
      posix::Stat(path.c_str(), &file_stat) == 0 && posix::IsDir(file_stat);
#endif  // GTEST_OS_WINDOWS_MOBILE

  return result;
}

// Returns true if pathname describes a root directory. (Windows has one
// root directory per disk drive. UNC share roots are also included.)
bool FilePath::IsRootDirectory() const {
  size_t root_length = CalculateRootLength();
  return root_length > 0 && root_length == pathname_.size() &&
         IsPathSeparator(pathname_[root_length - 1]);
}

// Returns true if pathname describes an absolute path.
bool FilePath::IsAbsolutePath() const { return CalculateRootLength() > 0; }

// Returns a pathname for a file that does not currently exist. The pathname
// will be directory/base_name.extension or
// directory/base_name_<number>.extension if directory/base_name.extension
// already exists. The number will be incremented until a pathname is found
// that does not already exist.
// Examples: 'dir/foo_test.xml' or 'dir/foo_test_1.xml'.
// There could be a race condition if two or more processes are calling this
// function at the same time -- they could both pick the same filename.
FilePath FilePath::GenerateUniqueFileName(const FilePath& directory,
                                          const FilePath& base_name,
                                          const char* extension) {
  FilePath full_pathname;
  int number = 0;
  do {
    full_pathname.Set(MakeFileName(directory, base_name, number++, extension));
  } while (full_pathname.FileOrDirectoryExists());
  return full_pathname;
}

// Returns true if FilePath ends with a path separator, which indicates that
// it is intended to represent a directory. Returns false otherwise.
// This does NOT check that a directory (or file) actually exists.
bool FilePath::IsDirectory() const {
  return !pathname_.empty() &&
         IsPathSeparator(pathname_.c_str()[pathname_.length() - 1]);
}

// Create directories so that path exists. Returns true if successful or if
// the directories already exist; returns false if unable to create directories
// for any reason.
bool FilePath::CreateDirectoriesRecursively() const {
  if (!this->IsDirectory()) {
    return false;
  }

  if (pathname_.empty() || this->DirectoryExists()) {
    return true;
  }

  const FilePath parent(this->RemoveTrailingPathSeparator().RemoveFileName());
  return parent.CreateDirectoriesRecursively() && this->CreateFolder();
}

// Create the directory so that path exists. Returns true if successful or
// if the directory already exists; returns false if unable to create the
// directory for any reason, including if the parent directory does not
// exist. Not named "CreateDirectory" because that's a macro on Windows.
bool FilePath::CreateFolder() const {
#ifdef GTEST_OS_WINDOWS_MOBILE
  FilePath removed_sep(this->RemoveTrailingPathSeparator());
  LPCWSTR unicode = String::AnsiToUtf16(removed_sep.c_str());
  int result = CreateDirectory(unicode, nullptr) ? 0 : -1;
  delete[] unicode;
#elif defined(GTEST_OS_WINDOWS)
  int result = _mkdir(pathname_.c_str());
#elif defined(GTEST_OS_ESP8266) || defined(GTEST_OS_XTENSA) || \
    defined(GTEST_OS_QURT) || defined(GTEST_OS_NXP_QN9090) ||  \
    defined(GTEST_OS_NRF52)
  // do nothing
  int result = 0;
#else
  int result = mkdir(pathname_.c_str(), 0777);
#endif  // GTEST_OS_WINDOWS_MOBILE

  if (result == -1) {
    return this->DirectoryExists();  // An error is OK if the directory exists.
  }
  return true;  // No error.
}

// If input name has a trailing separator character, remove it and return the
// name, otherwise return the name string unmodified.
// On Windows platform, uses \ as the separator, other platforms use /.
FilePath FilePath::RemoveTrailingPathSeparator() const {
  return IsDirectory() ? FilePath(pathname_.substr(0, pathname_.length() - 1))
                       : *this;
}

// Removes any redundant separators that might be in the pathname.
// For example, "bar///foo" becomes "bar/foo". Does not eliminate other
// redundancies that might be in a pathname involving "." or "..".
// Note that "\\Host\Share" does not contain a redundancy on Windows!
void FilePath::Normalize() {
  auto out = pathname_.begin();

  auto i = pathname_.cbegin();
#ifdef GTEST_OS_WINDOWS
  // UNC paths are treated specially
  if (pathname_.end() - i >= 3 && IsPathSeparator(*i) &&
      IsPathSeparator(*(i + 1)) && !IsPathSeparator(*(i + 2))) {
    *(out++) = kPathSeparator;
    *(out++) = kPathSeparator;
  }
#endif
  while (i != pathname_.end()) {
    const char character = *i;
    if (!IsPathSeparator(character)) {
      *(out++) = character;
    } else if (out == pathname_.begin() || *std::prev(out) != kPathSeparator) {
      *(out++) = kPathSeparator;
    }
    ++i;
  }

  pathname_.erase(out, pathname_.end());
}

}  // namespace internal
}  // namespace testing

#endif  // GTEST_HAS_FILE_SYSTEM
