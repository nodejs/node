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

// Unit tests for long-path-aware open/mkdir/access/etc. on Windows, as well as
// for the supporting utility functions.
//
// This file is only used on Windows, it's empty on other platforms.

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wchar.h>
#include <windows.h>

#include <google/protobuf/io/io_win32.h>
#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

namespace google {
namespace protobuf {
namespace io {
namespace win32 {
namespace {

const char kUtf8Text[] = {
    'h', 'i', ' ',
    // utf-8: 11010000 10011111, utf-16: 100 0001 1111 = 0x041F
    static_cast<char>(0xd0), static_cast<char>(0x9f),
    // utf-8: 11010001 10000000, utf-16: 100 0100 0000 = 0x0440
    static_cast<char>(0xd1), static_cast<char>(0x80),
    // utf-8: 11010000 10111000, utf-16: 100 0011 1000 = 0x0438
    static_cast<char>(0xd0), static_cast<char>(0xb8),
    // utf-8: 11010000 10110010, utf-16: 100 0011 0010 = 0x0432
    static_cast<char>(0xd0), static_cast<char>(0xb2),
    // utf-8: 11010000 10110101, utf-16: 100 0011 0101 = 0x0435
    static_cast<char>(0xd0), static_cast<char>(0xb5),
    // utf-8: 11010001 10000010, utf-16: 100 0100 0010 = 0x0442
    static_cast<char>(0xd1), static_cast<char>(0x82), 0
};

const wchar_t kUtf16Text[] = {
  L'h', L'i', L' ',
  L'\x41f', L'\x440', L'\x438', L'\x432', L'\x435', L'\x442', 0
};

using std::string;
using std::wstring;

class IoWin32Test : public ::testing::Test {
 public:
  void SetUp();
  void TearDown();

 protected:
  bool CreateAllUnder(wstring path);
  bool DeleteAllUnder(wstring path);

  WCHAR working_directory[MAX_PATH];
  string test_tmpdir;
  wstring wtest_tmpdir;
};

#define ASSERT_INITIALIZED              \
  {                                     \
    EXPECT_FALSE(test_tmpdir.empty());  \
    EXPECT_FALSE(wtest_tmpdir.empty()); \
  }

namespace {
void StripTrailingSlashes(string* str) {
  int i = str->size() - 1;
  for (; i >= 0 && ((*str)[i] == '/' || (*str)[i] == '\\'); --i) {}
  str->resize(i+1);
}

bool GetEnvVarAsUtf8(const WCHAR* name, string* result) {
  DWORD size = ::GetEnvironmentVariableW(name, nullptr, 0);
  if (size > 0 && GetLastError() != ERROR_ENVVAR_NOT_FOUND) {
    std::unique_ptr<WCHAR[]> wcs(new WCHAR[size]);
    ::GetEnvironmentVariableW(name, wcs.get(), size);
    // GetEnvironmentVariableA retrieves an Active-Code-Page-encoded text which
    // we'd first need to convert to UTF-16 then to UTF-8, because there seems
    // to be no API function to do that conversion directly.
    // GetEnvironmentVariableW retrieves an UTF-16-encoded text, which we need
    // to convert to UTF-8.
    return strings::wcs_to_utf8(wcs.get(), result);
  } else {
    return false;
  }
}

bool GetCwdAsUtf8(string* result) {
  DWORD size = ::GetCurrentDirectoryW(0, nullptr);
  if (size > 0) {
    std::unique_ptr<WCHAR[]> wcs(new WCHAR[size]);
    ::GetCurrentDirectoryW(size, wcs.get());
    // GetCurrentDirectoryA retrieves an Active-Code-Page-encoded text which
    // we'd first need to convert to UTF-16 then to UTF-8, because there seems
    // to be no API function to do that conversion directly.
    // GetCurrentDirectoryW retrieves an UTF-16-encoded text, which we need
    // to convert to UTF-8.
    return strings::wcs_to_utf8(wcs.get(), result);
  } else {
    return false;
  }
}

}  // namespace

void IoWin32Test::SetUp() {
  test_tmpdir.clear();
  wtest_tmpdir.clear();
  EXPECT_GT(::GetCurrentDirectoryW(MAX_PATH, working_directory), 0);

  string tmp;
  bool ok = false;
  if (!ok) {
    // Bazel sets this environment variable when it runs tests.
    ok = GetEnvVarAsUtf8(L"TEST_TMPDIR", &tmp);
  }
  if (!ok) {
    // Bazel 0.8.0 sets this environment for every build and test action.
    ok = GetEnvVarAsUtf8(L"TEMP", &tmp);
  }
  if (!ok) {
    // Bazel 0.8.0 sets this environment for every build and test action.
    ok = GetEnvVarAsUtf8(L"TMP", &tmp);
  }
  if (!ok) {
    // Fall back to using the current directory.
    ok = GetCwdAsUtf8(&tmp);
  }
  if (!ok || tmp.empty()) {
    FAIL() << "Cannot find a temp directory.";
  }

  StripTrailingSlashes(&tmp);
  std::stringstream result;
  // Deleting files and directories is asynchronous on Windows, and if TearDown
  // just deleted the previous temp directory, sometimes we cannot recreate the
  // same directory.
  // Use a counter so every test method gets its own temp directory.
  static unsigned int counter = 0;
  result << tmp << "\\w32tst" << counter++ << ".tmp";
  test_tmpdir = result.str();
  wtest_tmpdir = testonly_utf8_to_winpath(test_tmpdir.c_str());
  ASSERT_FALSE(wtest_tmpdir.empty());
  ASSERT_TRUE(DeleteAllUnder(wtest_tmpdir));
  ASSERT_TRUE(CreateAllUnder(wtest_tmpdir));
}

void IoWin32Test::TearDown() {
  if (!wtest_tmpdir.empty()) {
    DeleteAllUnder(wtest_tmpdir);
  }
  ::SetCurrentDirectoryW(working_directory);
}

bool IoWin32Test::CreateAllUnder(wstring path) {
  // Prepend UNC prefix if the path doesn't have it already. Don't bother
  // checking if the path is shorter than MAX_PATH, let's just do it
  // unconditionally.
  if (path.find(L"\\\\?\\") != 0) {
    path = wstring(L"\\\\?\\") + path;
  }
  if (::CreateDirectoryW(path.c_str(), nullptr) ||
      GetLastError() == ERROR_ALREADY_EXISTS ||
      GetLastError() == ERROR_ACCESS_DENIED) {
    return true;
  }
  if (GetLastError() == ERROR_PATH_NOT_FOUND) {
    size_t pos = path.find_last_of(L'\\');
    if (pos != wstring::npos) {
      wstring parent(path, 0, pos);
      if (CreateAllUnder(parent) && CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
      }
    }
  }
  return false;
}

bool IoWin32Test::DeleteAllUnder(wstring path) {
  static const wstring kDot(L".");
  static const wstring kDotDot(L"..");

  // Prepend UNC prefix if the path doesn't have it already. Don't bother
  // checking if the path is shorter than MAX_PATH, let's just do it
  // unconditionally.
  if (path.find(L"\\\\?\\") != 0) {
    path = wstring(L"\\\\?\\") + path;
  }
  // Append "\" if necessary.
  if (path[path.size() - 1] != L'\\') {
    path.push_back(L'\\');
  }

  WIN32_FIND_DATAW metadata;
  HANDLE handle = ::FindFirstFileW((path + L"*").c_str(), &metadata);
  if (handle == INVALID_HANDLE_VALUE) {
    return true;  // directory doesn't exist
  }

  bool result = true;
  do {
    wstring childname = metadata.cFileName;
    if (kDot != childname && kDotDot != childname) {
      wstring childpath = path + childname;
      if ((metadata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        // If this is not a junction, delete its contents recursively.
        // Finally delete this directory/junction too.
        if (((metadata.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
             !DeleteAllUnder(childpath)) ||
            !::RemoveDirectoryW(childpath.c_str())) {
          result = false;
          break;
        }
      } else {
        if (!::DeleteFileW(childpath.c_str())) {
          result = false;
          break;
        }
      }
    }
  } while (::FindNextFileW(handle, &metadata));
  ::FindClose(handle);
  return result;
}

TEST_F(IoWin32Test, AccessTest) {
  ASSERT_INITIALIZED;

  string path = test_tmpdir;
  while (path.size() < MAX_PATH - 30) {
    path += "\\accesstest";
    EXPECT_EQ(mkdir(path.c_str(), 0644), 0);
  }
  string file = path + "\\file.txt";
  int fd = open(file.c_str(), O_CREAT | O_WRONLY, 0644);
  if (fd > 0) {
    EXPECT_EQ(close(fd), 0);
  } else {
    EXPECT_TRUE(false);
  }

  EXPECT_EQ(access(test_tmpdir.c_str(), F_OK), 0);
  EXPECT_EQ(access(path.c_str(), F_OK), 0);
  EXPECT_EQ(access(path.c_str(), W_OK), 0);
  EXPECT_EQ(access(file.c_str(), F_OK | W_OK), 0);
  EXPECT_NE(access((file + ".blah").c_str(), F_OK), 0);
  EXPECT_NE(access((file + ".blah").c_str(), W_OK), 0);

  EXPECT_EQ(access(".", F_OK), 0);
  EXPECT_EQ(access(".", W_OK), 0);
  EXPECT_EQ(access((test_tmpdir + "/accesstest").c_str(), F_OK | W_OK), 0);
  ASSERT_EQ(access((test_tmpdir + "/./normalize_me/.././accesstest").c_str(),
                   F_OK | W_OK),
            0);
  EXPECT_NE(access("io_win32_unittest.AccessTest.nonexistent", F_OK), 0);
  EXPECT_NE(access("io_win32_unittest.AccessTest.nonexistent", W_OK), 0);

  ASSERT_EQ(access("c:bad", F_OK), -1);
  ASSERT_EQ(errno, ENOENT);
  ASSERT_EQ(access("/tmp/bad", F_OK), -1);
  ASSERT_EQ(errno, ENOENT);
  ASSERT_EQ(access("\\bad", F_OK), -1);
  ASSERT_EQ(errno, ENOENT);
}

TEST_F(IoWin32Test, OpenTest) {
  ASSERT_INITIALIZED;

  string path = test_tmpdir;
  while (path.size() < MAX_PATH) {
    path += "\\opentest";
    EXPECT_EQ(mkdir(path.c_str(), 0644), 0);
  }
  string file = path + "\\file.txt";
  int fd = open(file.c_str(), O_CREAT | O_WRONLY, 0644);
  if (fd > 0) {
    EXPECT_EQ(write(fd, "hello", 5), 5);
    EXPECT_EQ(close(fd), 0);
  } else {
    EXPECT_TRUE(false);
  }

  ASSERT_EQ(open("c:bad.txt", O_CREAT | O_WRONLY, 0644), -1);
  ASSERT_EQ(errno, ENOENT);
  ASSERT_EQ(open("/tmp/bad.txt", O_CREAT | O_WRONLY, 0644), -1);
  ASSERT_EQ(errno, ENOENT);
  ASSERT_EQ(open("\\bad.txt", O_CREAT | O_WRONLY, 0644), -1);
  ASSERT_EQ(errno, ENOENT);
}

TEST_F(IoWin32Test, MkdirTest) {
  ASSERT_INITIALIZED;

  string path = test_tmpdir;
  do {
    path += "\\mkdirtest";
    ASSERT_EQ(mkdir(path.c_str(), 0644), 0);
  } while (path.size() <= MAX_PATH);

  ASSERT_EQ(mkdir("c:bad", 0644), -1);
  ASSERT_EQ(errno, ENOENT);
  ASSERT_EQ(mkdir("/tmp/bad", 0644), -1);
  ASSERT_EQ(errno, ENOENT);
  ASSERT_EQ(mkdir("\\bad", 0644), -1);
  ASSERT_EQ(errno, ENOENT);
}

TEST_F(IoWin32Test, MkdirTestNonAscii) {
  ASSERT_INITIALIZED;

  // Create a non-ASCII path.
  // Ensure that we can create the directory using SetCurrentDirectoryW.
  EXPECT_TRUE(CreateDirectoryW((wtest_tmpdir + L"\\1").c_str(), nullptr));
  EXPECT_TRUE(CreateDirectoryW((wtest_tmpdir + L"\\1\\" + kUtf16Text).c_str(), nullptr));
  // Ensure that we can create a very similarly named directory using mkdir.
  // We don't attemp to delete and recreate the same directory, because on
  // Windows, deleting files and directories seems to be asynchronous.
  EXPECT_EQ(mkdir((test_tmpdir + "\\2").c_str(), 0644), 0);
  EXPECT_EQ(mkdir((test_tmpdir + "\\2\\" + kUtf8Text).c_str(), 0644), 0);
}

TEST_F(IoWin32Test, ChdirTest) {
  string path("C:\\");
  EXPECT_EQ(access(path.c_str(), F_OK), 0);
  ASSERT_EQ(chdir(path.c_str()), 0);

  // Do not try to chdir into the test_tmpdir, it may already contain directory
  // names with trailing dots.
  // Instead test here with an obviously dot-trailed path. If the win32_chdir
  // function would not convert the path to absolute and prefix with "\\?\" then
  // the Win32 API would ignore the trailing dot, but because of the prefixing
  // there'll be no path processing done, so we'll actually attempt to chdir
  // into "C:\some\path\foo."
  path = test_tmpdir + "/foo.";
  EXPECT_EQ(mkdir(path.c_str(), 644), 0);
  EXPECT_EQ(access(path.c_str(), F_OK), 0);
  ASSERT_NE(chdir(path.c_str()), 0);
}

TEST_F(IoWin32Test, ChdirTestNonAscii) {
  ASSERT_INITIALIZED;

  // Create a directory with a non-ASCII path and ensure we can cd into it.
  wstring wNonAscii(wtest_tmpdir + L"\\" + kUtf16Text);
  string nonAscii;
  EXPECT_TRUE(strings::wcs_to_utf8(wNonAscii.c_str(), &nonAscii));
  EXPECT_TRUE(CreateDirectoryW(wNonAscii.c_str(), nullptr));
  WCHAR cwd[MAX_PATH];
  EXPECT_TRUE(GetCurrentDirectoryW(MAX_PATH, cwd));
  // Ensure that we can cd into the path using SetCurrentDirectoryW.
  EXPECT_TRUE(SetCurrentDirectoryW(wNonAscii.c_str()));
  EXPECT_TRUE(SetCurrentDirectoryW(cwd));
  // Ensure that we can cd into the path using chdir.
  ASSERT_EQ(chdir(nonAscii.c_str()), 0);
  // Ensure that the GetCurrentDirectoryW returns the desired path.
  EXPECT_TRUE(GetCurrentDirectoryW(MAX_PATH, cwd));
  ASSERT_EQ(wNonAscii, cwd);
}

TEST_F(IoWin32Test, AsWindowsPathTest) {
  DWORD size = GetCurrentDirectoryW(0, nullptr);
  std::unique_ptr<wchar_t[]> cwd_str(new wchar_t[size]);
  EXPECT_GT(GetCurrentDirectoryW(size, cwd_str.get()), 0);
  wstring cwd = wstring(L"\\\\?\\") + cwd_str.get();

  ASSERT_EQ(testonly_utf8_to_winpath("relative_mkdirtest"),
            cwd + L"\\relative_mkdirtest");
  ASSERT_EQ(testonly_utf8_to_winpath("preserve//\\trailing///"),
            cwd + L"\\preserve\\trailing\\");
  ASSERT_EQ(testonly_utf8_to_winpath("./normalize_me\\/../blah"),
            cwd + L"\\blah");
  std::ostringstream relpath;
  for (wchar_t* p = cwd_str.get(); *p; ++p) {
    if (*p == '/' || *p == '\\') {
      relpath << "../";
    }
  }
  relpath << ".\\/../\\./beyond-toplevel";
  ASSERT_EQ(testonly_utf8_to_winpath(relpath.str().c_str()),
            wstring(L"\\\\?\\") + cwd_str.get()[0] + L":\\beyond-toplevel");

  // Absolute unix paths lack drive letters, driveless absolute windows paths
  // do too. Neither can be converted to a drive-specifying absolute Windows
  // path.
  ASSERT_EQ(testonly_utf8_to_winpath("/absolute/unix/path"), L"");
  // Though valid on Windows, we also don't support UNC paths (\\UNC\\blah).
  ASSERT_EQ(testonly_utf8_to_winpath("\\driveless\\absolute"), L"");
  // Though valid in cmd.exe, drive-relative paths are not supported.
  ASSERT_EQ(testonly_utf8_to_winpath("c:foo"), L"");
  ASSERT_EQ(testonly_utf8_to_winpath("c:/foo"), L"\\\\?\\c:\\foo");
  ASSERT_EQ(testonly_utf8_to_winpath("\\\\?\\C:\\foo"), L"\\\\?\\C:\\foo");
}

TEST_F(IoWin32Test, Utf8Utf16ConversionTest) {
  string mbs;
  wstring wcs;
  ASSERT_TRUE(strings::utf8_to_wcs(kUtf8Text, &wcs));
  ASSERT_TRUE(strings::wcs_to_utf8(kUtf16Text, &mbs));
  ASSERT_EQ(wcs, kUtf16Text);
  ASSERT_EQ(mbs, kUtf8Text);
}

}  // namespace
}  // namespace win32
}  // namespace io
}  // namespace protobuf
}  // namespace google

#endif  // defined(_WIN32)

