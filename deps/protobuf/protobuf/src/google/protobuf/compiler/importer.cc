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
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#ifdef _MSC_VER
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <memory>

#include <google/protobuf/compiler/importer.h>

#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/io/io_win32.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/stubs/strutil.h>



#ifdef _WIN32
#include <ctype.h>
#endif

namespace google {
namespace protobuf {
namespace compiler {

#ifdef _WIN32
// DO NOT include <io.h>, instead create functions in io_win32.{h,cc} and import
// them like we do below.
using google::protobuf::io::win32::access;
using google::protobuf::io::win32::open;
#endif

// Returns true if the text looks like a Windows-style absolute path, starting
// with a drive letter.  Example:  "C:\foo".  TODO(kenton):  Share this with
// copy in command_line_interface.cc?
static bool IsWindowsAbsolutePath(const std::string& text) {
#if defined(_WIN32) || defined(__CYGWIN__)
  return text.size() >= 3 && text[1] == ':' && isalpha(text[0]) &&
         (text[2] == '/' || text[2] == '\\') && text.find_last_of(':') == 1;
#else
  return false;
#endif
}

MultiFileErrorCollector::~MultiFileErrorCollector() {}

// This class serves two purposes:
// - It implements the ErrorCollector interface (used by Tokenizer and Parser)
//   in terms of MultiFileErrorCollector, using a particular filename.
// - It lets us check if any errors have occurred.
class SourceTreeDescriptorDatabase::SingleFileErrorCollector
    : public io::ErrorCollector {
 public:
  SingleFileErrorCollector(const std::string& filename,
                           MultiFileErrorCollector* multi_file_error_collector)
      : filename_(filename),
        multi_file_error_collector_(multi_file_error_collector),
        had_errors_(false) {}
  ~SingleFileErrorCollector() {}

  bool had_errors() { return had_errors_; }

  // implements ErrorCollector ---------------------------------------
  void AddError(int line, int column, const std::string& message) override {
    if (multi_file_error_collector_ != NULL) {
      multi_file_error_collector_->AddError(filename_, line, column, message);
    }
    had_errors_ = true;
  }

 private:
  std::string filename_;
  MultiFileErrorCollector* multi_file_error_collector_;
  bool had_errors_;
};

// ===================================================================

SourceTreeDescriptorDatabase::SourceTreeDescriptorDatabase(
    SourceTree* source_tree)
    : source_tree_(source_tree),
      fallback_database_(nullptr),
      error_collector_(nullptr),
      using_validation_error_collector_(false),
      validation_error_collector_(this) {}

SourceTreeDescriptorDatabase::SourceTreeDescriptorDatabase(
    SourceTree* source_tree, DescriptorDatabase* fallback_database)
    : source_tree_(source_tree),
      fallback_database_(fallback_database),
      error_collector_(nullptr),
      using_validation_error_collector_(false),
      validation_error_collector_(this) {}

SourceTreeDescriptorDatabase::~SourceTreeDescriptorDatabase() {}

bool SourceTreeDescriptorDatabase::FindFileByName(const std::string& filename,
                                                  FileDescriptorProto* output) {
  std::unique_ptr<io::ZeroCopyInputStream> input(source_tree_->Open(filename));
  if (input == NULL) {
    if (fallback_database_ != nullptr &&
        fallback_database_->FindFileByName(filename, output)) {
      return true;
    }
    if (error_collector_ != NULL) {
      error_collector_->AddError(filename, -1, 0,
                                 source_tree_->GetLastErrorMessage());
    }
    return false;
  }

  // Set up the tokenizer and parser.
  SingleFileErrorCollector file_error_collector(filename, error_collector_);
  io::Tokenizer tokenizer(input.get(), &file_error_collector);

  Parser parser;
  if (error_collector_ != NULL) {
    parser.RecordErrorsTo(&file_error_collector);
  }
  if (using_validation_error_collector_) {
    parser.RecordSourceLocationsTo(&source_locations_);
  }

  // Parse it.
  output->set_name(filename);
  return parser.Parse(&tokenizer, output) && !file_error_collector.had_errors();
}

bool SourceTreeDescriptorDatabase::FindFileContainingSymbol(
    const std::string& symbol_name, FileDescriptorProto* output) {
  return false;
}

bool SourceTreeDescriptorDatabase::FindFileContainingExtension(
    const std::string& containing_type, int field_number,
    FileDescriptorProto* output) {
  return false;
}

// -------------------------------------------------------------------

SourceTreeDescriptorDatabase::ValidationErrorCollector::
    ValidationErrorCollector(SourceTreeDescriptorDatabase* owner)
    : owner_(owner) {}

SourceTreeDescriptorDatabase::ValidationErrorCollector::
    ~ValidationErrorCollector() {}

void SourceTreeDescriptorDatabase::ValidationErrorCollector::AddError(
    const std::string& filename, const std::string& element_name,
    const Message* descriptor, ErrorLocation location,
    const std::string& message) {
  if (owner_->error_collector_ == NULL) return;

  int line, column;
  if (location == DescriptorPool::ErrorCollector::IMPORT) {
    owner_->source_locations_.FindImport(descriptor, element_name, &line,
                                         &column);
  } else {
    owner_->source_locations_.Find(descriptor, location, &line, &column);
  }
  owner_->error_collector_->AddError(filename, line, column, message);
}

void SourceTreeDescriptorDatabase::ValidationErrorCollector::AddWarning(
    const std::string& filename, const std::string& element_name,
    const Message* descriptor, ErrorLocation location,
    const std::string& message) {
  if (owner_->error_collector_ == NULL) return;

  int line, column;
  if (location == DescriptorPool::ErrorCollector::IMPORT) {
    owner_->source_locations_.FindImport(descriptor, element_name, &line,
                                         &column);
  } else {
    owner_->source_locations_.Find(descriptor, location, &line, &column);
  }
  owner_->error_collector_->AddWarning(filename, line, column, message);
}

// ===================================================================

Importer::Importer(SourceTree* source_tree,
                   MultiFileErrorCollector* error_collector)
    : database_(source_tree),
      pool_(&database_, database_.GetValidationErrorCollector()) {
  pool_.EnforceWeakDependencies(true);
  database_.RecordErrorsTo(error_collector);
}

Importer::~Importer() {}

const FileDescriptor* Importer::Import(const std::string& filename) {
  return pool_.FindFileByName(filename);
}

void Importer::AddUnusedImportTrackFile(const std::string& file_name) {
  pool_.AddUnusedImportTrackFile(file_name);
}

void Importer::ClearUnusedImportTrackFiles() {
  pool_.ClearUnusedImportTrackFiles();
}


// ===================================================================

SourceTree::~SourceTree() {}

std::string SourceTree::GetLastErrorMessage() { return "File not found."; }

DiskSourceTree::DiskSourceTree() {}

DiskSourceTree::~DiskSourceTree() {}

static inline char LastChar(const std::string& str) {
  return str[str.size() - 1];
}

// Given a path, returns an equivalent path with these changes:
// - On Windows, any backslashes are replaced with forward slashes.
// - Any instances of the directory "." are removed.
// - Any consecutive '/'s are collapsed into a single slash.
// Note that the resulting string may be empty.
//
// TODO(kenton):  It would be nice to handle "..", e.g. so that we can figure
//   out that "foo/bar.proto" is inside "baz/../foo".  However, if baz is a
//   symlink or doesn't exist, then things get complicated, and we can't
//   actually determine this without investigating the filesystem, probably
//   in non-portable ways.  So, we punt.
//
// TODO(kenton):  It would be nice to use realpath() here except that it
//   resolves symbolic links.  This could cause problems if people place
//   symbolic links in their source tree.  For example, if you executed:
//     protoc --proto_path=foo foo/bar/baz.proto
//   then if foo/bar is a symbolic link, foo/bar/baz.proto will canonicalize
//   to a path which does not appear to be under foo, and thus the compiler
//   will complain that baz.proto is not inside the --proto_path.
static std::string CanonicalizePath(std::string path) {
#ifdef _WIN32
  // The Win32 API accepts forward slashes as a path delimiter even though
  // backslashes are standard.  Let's avoid confusion and use only forward
  // slashes.
  if (HasPrefixString(path, "\\\\")) {
    // Avoid converting two leading backslashes.
    path = "\\\\" + StringReplace(path.substr(2), "\\", "/", true);
  } else {
    path = StringReplace(path, "\\", "/", true);
  }
#endif

  std::vector<std::string> canonical_parts;
  std::vector<std::string> parts = Split(
      path, "/", true);  // Note:  Removes empty parts.
  for (int i = 0; i < parts.size(); i++) {
    if (parts[i] == ".") {
      // Ignore.
    } else {
      canonical_parts.push_back(parts[i]);
    }
  }
  std::string result = Join(canonical_parts, "/");
  if (!path.empty() && path[0] == '/') {
    // Restore leading slash.
    result = '/' + result;
  }
  if (!path.empty() && LastChar(path) == '/' && !result.empty() &&
      LastChar(result) != '/') {
    // Restore trailing slash.
    result += '/';
  }
  return result;
}

static inline bool ContainsParentReference(const std::string& path) {
  return path == ".." || HasPrefixString(path, "../") ||
         HasSuffixString(path, "/..") || path.find("/../") != string::npos;
}

// Maps a file from an old location to a new one.  Typically, old_prefix is
// a virtual path and new_prefix is its corresponding disk path.  Returns
// false if the filename did not start with old_prefix, otherwise replaces
// old_prefix with new_prefix and stores the result in *result.  Examples:
//   string result;
//   assert(ApplyMapping("foo/bar", "", "baz", &result));
//   assert(result == "baz/foo/bar");
//
//   assert(ApplyMapping("foo/bar", "foo", "baz", &result));
//   assert(result == "baz/bar");
//
//   assert(ApplyMapping("foo", "foo", "bar", &result));
//   assert(result == "bar");
//
//   assert(!ApplyMapping("foo/bar", "baz", "qux", &result));
//   assert(!ApplyMapping("foo/bar", "baz", "qux", &result));
//   assert(!ApplyMapping("foobar", "foo", "baz", &result));
static bool ApplyMapping(const std::string& filename,
                         const std::string& old_prefix,
                         const std::string& new_prefix, std::string* result) {
  if (old_prefix.empty()) {
    // old_prefix matches any relative path.
    if (ContainsParentReference(filename)) {
      // We do not allow the file name to use "..".
      return false;
    }
    if (HasPrefixString(filename, "/") || IsWindowsAbsolutePath(filename)) {
      // This is an absolute path, so it isn't matched by the empty string.
      return false;
    }
    result->assign(new_prefix);
    if (!result->empty()) result->push_back('/');
    result->append(filename);
    return true;
  } else if (HasPrefixString(filename, old_prefix)) {
    // old_prefix is a prefix of the filename.  Is it the whole filename?
    if (filename.size() == old_prefix.size()) {
      // Yep, it's an exact match.
      *result = new_prefix;
      return true;
    } else {
      // Not an exact match.  Is the next character a '/'?  Otherwise,
      // this isn't actually a match at all.  E.g. the prefix "foo/bar"
      // does not match the filename "foo/barbaz".
      int after_prefix_start = -1;
      if (filename[old_prefix.size()] == '/') {
        after_prefix_start = old_prefix.size() + 1;
      } else if (filename[old_prefix.size() - 1] == '/') {
        // old_prefix is never empty, and canonicalized paths never have
        // consecutive '/' characters.
        after_prefix_start = old_prefix.size();
      }
      if (after_prefix_start != -1) {
        // Yep.  So the prefixes are directories and the filename is a file
        // inside them.
        std::string after_prefix = filename.substr(after_prefix_start);
        if (ContainsParentReference(after_prefix)) {
          // We do not allow the file name to use "..".
          return false;
        }
        result->assign(new_prefix);
        if (!result->empty()) result->push_back('/');
        result->append(after_prefix);
        return true;
      }
    }
  }

  return false;
}

void DiskSourceTree::MapPath(const std::string& virtual_path,
                             const std::string& disk_path) {
  mappings_.push_back(Mapping(virtual_path, CanonicalizePath(disk_path)));
}

DiskSourceTree::DiskFileToVirtualFileResult
DiskSourceTree::DiskFileToVirtualFile(const std::string& disk_file,
                                      std::string* virtual_file,
                                      std::string* shadowing_disk_file) {
  int mapping_index = -1;
  std::string canonical_disk_file = CanonicalizePath(disk_file);

  for (int i = 0; i < mappings_.size(); i++) {
    // Apply the mapping in reverse.
    if (ApplyMapping(canonical_disk_file, mappings_[i].disk_path,
                     mappings_[i].virtual_path, virtual_file)) {
      // Success.
      mapping_index = i;
      break;
    }
  }

  if (mapping_index == -1) {
    return NO_MAPPING;
  }

  // Iterate through all mappings with higher precedence and verify that none
  // of them map this file to some other existing file.
  for (int i = 0; i < mapping_index; i++) {
    if (ApplyMapping(*virtual_file, mappings_[i].virtual_path,
                     mappings_[i].disk_path, shadowing_disk_file)) {
      if (access(shadowing_disk_file->c_str(), F_OK) >= 0) {
        // File exists.
        return SHADOWED;
      }
    }
  }
  shadowing_disk_file->clear();

  // Verify that we can open the file.  Note that this also has the side-effect
  // of verifying that we are not canonicalizing away any non-existent
  // directories.
  std::unique_ptr<io::ZeroCopyInputStream> stream(OpenDiskFile(disk_file));
  if (stream == NULL) {
    return CANNOT_OPEN;
  }

  return SUCCESS;
}

bool DiskSourceTree::VirtualFileToDiskFile(const std::string& virtual_file,
                                           std::string* disk_file) {
  std::unique_ptr<io::ZeroCopyInputStream> stream(
      OpenVirtualFile(virtual_file, disk_file));
  return stream != NULL;
}

io::ZeroCopyInputStream* DiskSourceTree::Open(const std::string& filename) {
  return OpenVirtualFile(filename, NULL);
}

std::string DiskSourceTree::GetLastErrorMessage() {
  return last_error_message_;
}

io::ZeroCopyInputStream* DiskSourceTree::OpenVirtualFile(
    const std::string& virtual_file, std::string* disk_file) {
  if (virtual_file != CanonicalizePath(virtual_file) ||
      ContainsParentReference(virtual_file)) {
    // We do not allow importing of paths containing things like ".." or
    // consecutive slashes since the compiler expects files to be uniquely
    // identified by file name.
    last_error_message_ =
        "Backslashes, consecutive slashes, \".\", or \"..\" "
        "are not allowed in the virtual path";
    return NULL;
  }

  for (int i = 0; i < mappings_.size(); i++) {
    std::string temp_disk_file;
    if (ApplyMapping(virtual_file, mappings_[i].virtual_path,
                     mappings_[i].disk_path, &temp_disk_file)) {
      io::ZeroCopyInputStream* stream = OpenDiskFile(temp_disk_file);
      if (stream != NULL) {
        if (disk_file != NULL) {
          *disk_file = temp_disk_file;
        }
        return stream;
      }

      if (errno == EACCES) {
        // The file exists but is not readable.
        last_error_message_ =
            "Read access is denied for file: " + temp_disk_file;
        return NULL;
      }
    }
  }
  last_error_message_ = "File not found.";
  return NULL;
}

io::ZeroCopyInputStream* DiskSourceTree::OpenDiskFile(
    const std::string& filename) {
  int file_descriptor;
  do {
    file_descriptor = open(filename.c_str(), O_RDONLY);
  } while (file_descriptor < 0 && errno == EINTR);
  if (file_descriptor >= 0) {
    io::FileInputStream* result = new io::FileInputStream(file_descriptor);
    result->SetCloseOnDelete(true);
    return result;
  } else {
    return NULL;
  }
}

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
