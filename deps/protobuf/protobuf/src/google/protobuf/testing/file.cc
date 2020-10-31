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

// Author: kenton@google.com (Kenton Varda)
// emulates google3/file/base/file.cc

#include <google/protobuf/testing/file.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN  // yeah, right
#include <windows.h>         // Find*File().  :(
// #include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif
#include <errno.h>

#include <google/protobuf/io/io_win32.h>
#include <google/protobuf/stubs/logging.h>

namespace google {
namespace protobuf {

#ifdef _WIN32
// Windows doesn't have symbolic links.
#define lstat stat
// DO NOT include <io.h>, instead create functions in io_win32.{h,cc} and import
// them like we do below.
#endif

#ifdef _WIN32
using google::protobuf::io::win32::access;
using google::protobuf::io::win32::chdir;
using google::protobuf::io::win32::fopen;
using google::protobuf::io::win32::mkdir;
using google::protobuf::io::win32::stat;
#endif

bool File::Exists(const string& name) {
  return access(name.c_str(), F_OK) == 0;
}

bool File::ReadFileToString(const string& name, string* output, bool text_mode) {
  char buffer[1024];
  FILE* file = fopen(name.c_str(), text_mode ? "rt" : "rb");
  if (file == NULL) return false;

  while (true) {
    size_t n = fread(buffer, 1, sizeof(buffer), file);
    if (n <= 0) break;
    output->append(buffer, n);
  }

  int error = ferror(file);
  if (fclose(file) != 0) return false;
  return error == 0;
}

void File::ReadFileToStringOrDie(const string& name, string* output) {
  GOOGLE_CHECK(ReadFileToString(name, output)) << "Could not read: " << name;
}

bool File::WriteStringToFile(const string& contents, const string& name) {
  FILE* file = fopen(name.c_str(), "wb");
  if (file == NULL) {
    GOOGLE_LOG(ERROR) << "fopen(" << name << ", \"wb\"): " << strerror(errno);
    return false;
  }

  if (fwrite(contents.data(), 1, contents.size(), file) != contents.size()) {
    GOOGLE_LOG(ERROR) << "fwrite(" << name << "): " << strerror(errno);
    fclose(file);
    return false;
  }

  if (fclose(file) != 0) {
    return false;
  }
  return true;
}

void File::WriteStringToFileOrDie(const string& contents, const string& name) {
  FILE* file = fopen(name.c_str(), "wb");
  GOOGLE_CHECK(file != NULL)
      << "fopen(" << name << ", \"wb\"): " << strerror(errno);
  GOOGLE_CHECK_EQ(fwrite(contents.data(), 1, contents.size(), file),
                  contents.size())
      << "fwrite(" << name << "): " << strerror(errno);
  GOOGLE_CHECK(fclose(file) == 0)
      << "fclose(" << name << "): " << strerror(errno);
}

bool File::CreateDir(const string& name, int mode) {
  if (!name.empty()) {
    GOOGLE_CHECK_OK(name[name.size() - 1] != '.');
  }
  return mkdir(name.c_str(), mode) == 0;
}

bool File::RecursivelyCreateDir(const string& path, int mode) {
  if (CreateDir(path, mode)) return true;

  if (Exists(path)) return false;

  // Try creating the parent.
  string::size_type slashpos = path.find_last_of('/');
  if (slashpos == string::npos) {
    // No parent given.
    return false;
  }

  return RecursivelyCreateDir(path.substr(0, slashpos), mode) &&
         CreateDir(path, mode);
}

void File::DeleteRecursively(const string& name,
                             void* dummy1, void* dummy2) {
  if (name.empty()) return;

  // We don't care too much about error checking here since this is only used
  // in tests to delete temporary directories that are under /tmp anyway.

#ifdef _MSC_VER
  // This interface is so weird.
  WIN32_FIND_DATAA find_data;
  HANDLE find_handle = FindFirstFileA((name + "/*").c_str(), &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    // Just delete it, whatever it is.
    DeleteFileA(name.c_str());
    RemoveDirectoryA(name.c_str());
    return;
  }

  do {
    string entry_name = find_data.cFileName;
    if (entry_name != "." && entry_name != "..") {
      string path = name + "/" + entry_name;
      if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        DeleteRecursively(path, NULL, NULL);
        RemoveDirectoryA(path.c_str());
      } else {
        DeleteFileA(path.c_str());
      }
    }
  } while(FindNextFileA(find_handle, &find_data));
  FindClose(find_handle);

  RemoveDirectoryA(name.c_str());
#else
  // Use opendir()!  Yay!
  // lstat = Don't follow symbolic links.
  struct stat stats;
  if (lstat(name.c_str(), &stats) != 0) return;

  if (S_ISDIR(stats.st_mode)) {
    DIR* dir = opendir(name.c_str());
    if (dir != NULL) {
      while (true) {
        struct dirent* entry = readdir(dir);
        if (entry == NULL) break;
        string entry_name = entry->d_name;
        if (entry_name != "." && entry_name != "..") {
          DeleteRecursively(name + "/" + entry_name, NULL, NULL);
        }
      }
    }

    closedir(dir);
    rmdir(name.c_str());

  } else if (S_ISREG(stats.st_mode)) {
    remove(name.c_str());
  }
#endif
}

bool File::ChangeWorkingDirectory(const string& new_working_directory) {
  return chdir(new_working_directory.c_str()) == 0;
}

}  // namespace protobuf
}  // namespace google
