// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
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

// Author: laszlocsomor@google.com (Laszlo Csomor)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

// Implementation for long-path-aware open/mkdir/access/etc. on Windows, as well
// as for the supporting utility functions.
//
// These functions convert the input path to an absolute Windows path
// with "\\?\" prefix, then pass that to _wopen/_wmkdir/_waccess/etc.
// (declared in <io.h>) respectively. This allows working with files/directories
// whose paths are longer than MAX_PATH (260 chars).
//
// This file is only used on Windows, it's empty on other platforms.

#if defined(_WIN32) && !defined(_XBOX_ONE)

// Comment this out to fall back to using the ANSI versions (open, mkdir, ...)
// instead of the Unicode ones (_wopen, _wmkdir, ...). Doing so can be useful to
// debug failing tests if that's caused by the long path support.
#define SUPPORT_LONGPATHS

#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <wctype.h>
#include <windows.h>

#include <google/protobuf/io/io_win32.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace google {
namespace protobuf {
namespace io {
namespace win32 {
namespace {

using std::string;
using std::wstring;

template <typename char_type>
struct CharTraits {
  static bool is_alpha(char_type ch);
};

template <>
struct CharTraits<char> {
  static bool is_alpha(char ch) { return isalpha(ch); }
};

template <>
struct CharTraits<wchar_t> {
  static bool is_alpha(wchar_t ch) { return iswalpha(ch); }
};

template <typename char_type>
bool null_or_empty(const char_type* s) {
  return s == nullptr || *s == 0;
}

// Returns true if the path starts with a drive letter, e.g. "c:".
// Note that this won't check for the "\" after the drive letter, so this also
// returns true for "c:foo" (which is "c:\${PWD}\foo").
// This check requires that a path not have a longpath prefix ("\\?\").
template <typename char_type>
bool has_drive_letter(const char_type* ch) {
  return CharTraits<char_type>::is_alpha(ch[0]) && ch[1] == ':';
}

// Returns true if the path starts with a longpath prefix ("\\?\").
template <typename char_type>
bool has_longpath_prefix(const char_type* path) {
  return path[0] == '\\' && path[1] == '\\' && path[2] == '?' &&
         path[3] == '\\';
}

template <typename char_type>
bool is_separator(char_type c) {
  return c == '/' || c == '\\';
}

// Returns true if the path starts with a drive specifier (e.g. "c:\").
template <typename char_type>
bool is_path_absolute(const char_type* path) {
  return has_drive_letter(path) && is_separator(path[2]);
}

template <typename char_type>
bool is_drive_relative(const char_type* path) {
  return has_drive_letter(path) && (path[2] == 0 || !is_separator(path[2]));
}

wstring join_paths(const wstring& path1, const wstring& path2) {
  if (path1.empty() || is_path_absolute(path2.c_str()) ||
      has_longpath_prefix(path2.c_str())) {
    return path2;
  }
  if (path2.empty()) {
    return path1;
  }

  if (is_separator(path1[path1.size() - 1])) {
    return is_separator(path2[0]) ? (path1 + path2.substr(1))
                                       : (path1 + path2);
  } else {
    return is_separator(path2[0]) ? (path1 + path2)
                                       : (path1 + L'\\' + path2);
  }
}

wstring normalize(wstring path) {
  if (has_longpath_prefix(path.c_str())) {
    path = path.substr(4);
  }

  static const wstring dot(L".");
  static const wstring dotdot(L"..");
  const WCHAR* p = path.c_str();

  std::vector<wstring> segments;
  int segment_start = -1;
  // Find the path segments in `path` (separated by "/").
  for (int i = 0;; ++i) {
    if (!is_separator(p[i]) && p[i] != L'\0') {
      // The current character does not end a segment, so start one unless it's
      // already started.
      if (segment_start < 0) {
        segment_start = i;
      }
    } else if (segment_start >= 0 && i > segment_start) {
      // The current character is "/" or "\0", so this ends a segment.
      // Add that to `segments` if there's anything to add; handle "." and "..".
      wstring segment(p, segment_start, i - segment_start);
      segment_start = -1;
      if (segment == dotdot) {
        if (!segments.empty() &&
            (!has_drive_letter(segments[0].c_str()) || segments.size() > 1)) {
          segments.pop_back();
        }
      } else if (segment != dot && !segment.empty()) {
        segments.push_back(segment);
      }
    }
    if (p[i] == L'\0') {
      break;
    }
  }

  // Handle the case when `path` is just a drive specifier (or some degenerate
  // form of it, e.g. "c:\..").
  if (segments.size() == 1 && segments[0].size() == 2 &&
      has_drive_letter(segments[0].c_str())) {
    return segments[0] + L'\\';
  }

  // Join all segments.
  bool first = true;
  std::wstringstream result;
  for (int i = 0; i < segments.size(); ++i) {
    if (!first) {
      result << L'\\';
    }
    first = false;
    result << segments[i];
  }
  // Preserve trailing separator if the input contained it.
  if (!path.empty() && is_separator(p[path.size() - 1])) {
    result << L'\\';
  }
  return result.str();
}

bool as_windows_path(const char* path, wstring* result) {
  if (null_or_empty(path)) {
    result->clear();
    return true;
  }
  wstring wpath;
  if (!strings::utf8_to_wcs(path, &wpath)) {
    return false;
  }
  if (has_longpath_prefix(wpath.c_str())) {
    *result = wpath;
    return true;
  }
  if (is_separator(path[0]) || is_drive_relative(path)) {
    return false;
  }


  if (!is_path_absolute(wpath.c_str())) {
    int size = ::GetCurrentDirectoryW(0, nullptr);
    if (size == 0 && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      return false;
    }
    std::unique_ptr<WCHAR[]> wcwd(new WCHAR[size]);
    ::GetCurrentDirectoryW(size, wcwd.get());
    wpath = join_paths(wcwd.get(), wpath);
  }
  wpath = normalize(wpath);
  if (!has_longpath_prefix(wpath.c_str())) {
    // Add the "\\?\" prefix unconditionally. This way we prevent the Win32 API
    // from processing the path and "helpfully" removing trailing dots from the
    // path, for example.
    // See https://github.com/bazelbuild/bazel/issues/2935
    wpath = wstring(L"\\\\?\\") + wpath;
  }
  *result = wpath;
  return true;
}

}  // namespace

int open(const char* path, int flags, int mode) {
#ifdef SUPPORT_LONGPATHS
  wstring wpath;
  if (!as_windows_path(path, &wpath)) {
    errno = ENOENT;
    return -1;
  }
  return ::_wopen(wpath.c_str(), flags, mode);
#else
  return ::_open(path, flags, mode);
#endif
}

int mkdir(const char* path, int _mode) {
#ifdef SUPPORT_LONGPATHS
  wstring wpath;
  if (!as_windows_path(path, &wpath)) {
    errno = ENOENT;
    return -1;
  }
  return ::_wmkdir(wpath.c_str());
#else   // not SUPPORT_LONGPATHS
  return ::_mkdir(path);
#endif  // not SUPPORT_LONGPATHS
}

int access(const char* path, int mode) {
#ifdef SUPPORT_LONGPATHS
  wstring wpath;
  if (!as_windows_path(path, &wpath)) {
    errno = ENOENT;
    return -1;
  }
  return ::_waccess(wpath.c_str(), mode);
#else
  return ::_access(path, mode);
#endif
}

int chdir(const char* path) {
#ifdef SUPPORT_LONGPATHS
  wstring wpath;
  if (!as_windows_path(path, &wpath)) {
    errno = ENOENT;
    return -1;
  }
  return ::_wchdir(wpath.c_str());
#else
  return ::_chdir(path);
#endif
}

int stat(const char* path, struct _stat* buffer) {
#ifdef SUPPORT_LONGPATHS
  wstring wpath;
  if (!as_windows_path(path, &wpath)) {
    errno = ENOENT;
    return -1;
  }
  return ::_wstat(wpath.c_str(), buffer);
#else   // not SUPPORT_LONGPATHS
  return ::_stat(path, buffer);
#endif  // not SUPPORT_LONGPATHS
}

FILE* fopen(const char* path, const char* mode) {
#ifdef SUPPORT_LONGPATHS
  if (null_or_empty(path)) {
    errno = EINVAL;
    return nullptr;
  }
  wstring wpath;
  if (!as_windows_path(path, &wpath)) {
    errno = ENOENT;
    return nullptr;
  }
  wstring wmode;
  if (!strings::utf8_to_wcs(mode, &wmode)) {
    errno = EINVAL;
    return nullptr;
  }
  return ::_wfopen(wpath.c_str(), wmode.c_str());
#else
  return ::fopen(path, mode);
#endif
}

int close(int fd) { return ::_close(fd); }

int dup(int fd) { return ::_dup(fd); }

int dup2(int fd1, int fd2) { return ::_dup2(fd1, fd2); }

int read(int fd, void* buffer, size_t size) {
  return ::_read(fd, buffer, size);
}

int setmode(int fd, int mode) { return ::_setmode(fd, mode); }

int write(int fd, const void* buffer, size_t size) {
  return ::_write(fd, buffer, size);
}

wstring testonly_utf8_to_winpath(const char* path) {
  wstring wpath;
  return as_windows_path(path, &wpath) ? wpath : wstring();
}

namespace strings {

bool wcs_to_mbs(const WCHAR* s, string* out, bool outUtf8) {
  if (null_or_empty(s)) {
    out->clear();
    return true;
  }
  BOOL usedDefaultChar = FALSE;
  SetLastError(0);
  int size = WideCharToMultiByte(
      outUtf8 ? CP_UTF8 : CP_ACP, 0, s, -1, nullptr, 0, nullptr,
      outUtf8 ? nullptr : &usedDefaultChar);
  if ((size == 0 && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
      || usedDefaultChar) {
    return false;
  }
  std::unique_ptr<CHAR[]> astr(new CHAR[size]);
  WideCharToMultiByte(
      outUtf8 ? CP_UTF8 : CP_ACP, 0, s, -1, astr.get(), size, nullptr, nullptr);
  out->assign(astr.get());
  return true;
}

bool mbs_to_wcs(const char* s, wstring* out, bool inUtf8) {
  if (null_or_empty(s)) {
    out->clear();
    return true;
  }

  SetLastError(0);
  int size =
      MultiByteToWideChar(inUtf8 ? CP_UTF8 : CP_ACP, 0, s, -1, nullptr, 0);
  if (size == 0 && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return false;
  }
  std::unique_ptr<WCHAR[]> wstr(new WCHAR[size]);
  MultiByteToWideChar(
      inUtf8 ? CP_UTF8 : CP_ACP, 0, s, -1, wstr.get(), size + 1);
  out->assign(wstr.get());
  return true;
}

bool utf8_to_wcs(const char* input, wstring* out) {
  return mbs_to_wcs(input, out, true);
}

bool wcs_to_utf8(const wchar_t* input, string* out) {
  return wcs_to_mbs(input, out, true);
}

}  // namespace strings
}  // namespace win32
}  // namespace io
}  // namespace protobuf
}  // namespace google

#endif  // defined(_WIN32)
