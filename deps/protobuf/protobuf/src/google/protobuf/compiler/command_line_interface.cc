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

#include <google/protobuf/compiler/command_line_interface.h>


#include <google/protobuf/stubs/platform_macros.h>

#include <stdio.h>
#include <sys/types.h>
#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif
#include <fcntl.h>
#include <sys/stat.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fstream>
#include <iostream>

#include <limits.h>  //For PATH_MAX

#include <memory>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/compiler/subprocess.h>
#include <google/protobuf/compiler/zip_writer.h>
#include <google/protobuf/compiler/plugin.pb.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/io/io_win32.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/stubs/map_util.h>
#include <google/protobuf/stubs/stl_util.h>


#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace compiler {

#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0  // If this isn't defined, the platform doesn't need it.
#endif
#endif

namespace {
#if defined(_WIN32)
// DO NOT include <io.h>, instead create functions in io_win32.{h,cc} and import
// them like we do below.
using google::protobuf::io::win32::access;
using google::protobuf::io::win32::close;
using google::protobuf::io::win32::mkdir;
using google::protobuf::io::win32::open;
using google::protobuf::io::win32::setmode;
using google::protobuf::io::win32::write;
#endif

static const char* kDefaultDirectDependenciesViolationMsg =
    "File is imported but not declared in --direct_dependencies: %s";

// Returns true if the text looks like a Windows-style absolute path, starting
// with a drive letter.  Example:  "C:\foo".  TODO(kenton):  Share this with
// copy in importer.cc?
static bool IsWindowsAbsolutePath(const std::string& text) {
#if defined(_WIN32) || defined(__CYGWIN__)
  return text.size() >= 3 && text[1] == ':' && isalpha(text[0]) &&
         (text[2] == '/' || text[2] == '\\') && text.find_last_of(':') == 1;
#else
  return false;
#endif
}

void SetFdToTextMode(int fd) {
#ifdef _WIN32
  if (setmode(fd, _O_TEXT) == -1) {
    // This should never happen, I think.
    GOOGLE_LOG(WARNING) << "setmode(" << fd << ", _O_TEXT): " << strerror(errno);
  }
#endif
  // (Text and binary are the same on non-Windows platforms.)
}

void SetFdToBinaryMode(int fd) {
#ifdef _WIN32
  if (setmode(fd, _O_BINARY) == -1) {
    // This should never happen, I think.
    GOOGLE_LOG(WARNING) << "setmode(" << fd << ", _O_BINARY): " << strerror(errno);
  }
#endif
  // (Text and binary are the same on non-Windows platforms.)
}

void AddTrailingSlash(std::string* path) {
  if (!path->empty() && path->at(path->size() - 1) != '/') {
    path->push_back('/');
  }
}

bool VerifyDirectoryExists(const std::string& path) {
  if (path.empty()) return true;

  if (access(path.c_str(), F_OK) == -1) {
    std::cerr << path << ": " << strerror(errno) << std::endl;
    return false;
  } else {
    return true;
  }
}

// Try to create the parent directory of the given file, creating the parent's
// parent if necessary, and so on.  The full file name is actually
// (prefix + filename), but we assume |prefix| already exists and only create
// directories listed in |filename|.
bool TryCreateParentDirectory(const std::string& prefix,
                              const std::string& filename) {
  // Recursively create parent directories to the output file.
  // On Windows, both '/' and '\' are valid path separators.
  std::vector<std::string> parts =
      Split(filename, "/\\", true);
  std::string path_so_far = prefix;
  for (int i = 0; i < parts.size() - 1; i++) {
    path_so_far += parts[i];
    if (mkdir(path_so_far.c_str(), 0777) != 0) {
      if (errno != EEXIST) {
        std::cerr << filename << ": while trying to create directory "
                  << path_so_far << ": " << strerror(errno) << std::endl;
        return false;
      }
    }
    path_so_far += '/';
  }

  return true;
}

// Get the absolute path of this protoc binary.
bool GetProtocAbsolutePath(std::string* path) {
#ifdef _WIN32
  char buffer[MAX_PATH];
  int len = GetModuleFileNameA(NULL, buffer, MAX_PATH);
#elif defined(__APPLE__)
  char buffer[PATH_MAX];
  int len = 0;

  char dirtybuffer[PATH_MAX];
  uint32_t size = sizeof(dirtybuffer);
  if (_NSGetExecutablePath(dirtybuffer, &size) == 0) {
    realpath(dirtybuffer, buffer);
    len = strlen(buffer);
  }
#else
  char buffer[PATH_MAX];
  int len = readlink("/proc/self/exe", buffer, PATH_MAX);
#endif
  if (len > 0) {
    path->assign(buffer, len);
    return true;
  } else {
    return false;
  }
}

// Whether a path is where google/protobuf/descriptor.proto and other well-known
// type protos are installed.
bool IsInstalledProtoPath(const std::string& path) {
  // Checking the descriptor.proto file should be good enough.
  std::string file_path = path + "/google/protobuf/descriptor.proto";
  return access(file_path.c_str(), F_OK) != -1;
}

// Add the paths where google/protobuf/descriptor.proto and other well-known
// type protos are installed.
void AddDefaultProtoPaths(
    std::vector<std::pair<std::string, std::string> >* paths) {
  // TODO(xiaofeng): The code currently only checks relative paths of where
  // the protoc binary is installed. We probably should make it handle more
  // cases than that.
  std::string path;
  if (!GetProtocAbsolutePath(&path)) {
    return;
  }
  // Strip the binary name.
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos || pos == 0) {
    return;
  }
  path = path.substr(0, pos);
  // Check the binary's directory.
  if (IsInstalledProtoPath(path)) {
    paths->push_back(std::pair<std::string, std::string>("", path));
    return;
  }
  // Check if there is an include subdirectory.
  if (IsInstalledProtoPath(path + "/include")) {
    paths->push_back(
        std::pair<std::string, std::string>("", path + "/include"));
    return;
  }
  // Check if the upper level directory has an "include" subdirectory.
  pos = path.find_last_of("/\\");
  if (pos == std::string::npos || pos == 0) {
    return;
  }
  path = path.substr(0, pos);
  if (IsInstalledProtoPath(path + "/include")) {
    paths->push_back(
        std::pair<std::string, std::string>("", path + "/include"));
    return;
  }
}

string PluginName(const std::string& plugin_prefix,
                  const std::string& directive) {
  // Assuming the directive starts with "--" and ends with "_out" or "_opt",
  // strip the "--" and "_out/_opt" and add the plugin prefix.
  return plugin_prefix + "gen-" + directive.substr(2, directive.size() - 6);
}

}  // namespace

// A MultiFileErrorCollector that prints errors to stderr.
class CommandLineInterface::ErrorPrinter
    : public MultiFileErrorCollector,
      public io::ErrorCollector,
      public DescriptorPool::ErrorCollector {
 public:
  ErrorPrinter(ErrorFormat format, DiskSourceTree* tree = NULL)
      : format_(format), tree_(tree), found_errors_(false) {}
  ~ErrorPrinter() {}

  // implements MultiFileErrorCollector ------------------------------
  void AddError(const std::string& filename, int line, int column,
                const std::string& message) {
    found_errors_ = true;
    AddErrorOrWarning(filename, line, column, message, "error", std::cerr);
  }

  void AddWarning(const std::string& filename, int line, int column,
                  const std::string& message) {
    AddErrorOrWarning(filename, line, column, message, "warning", std::clog);
  }

  // implements io::ErrorCollector -----------------------------------
  void AddError(int line, int column, const std::string& message) {
    AddError("input", line, column, message);
  }

  void AddWarning(int line, int column, const std::string& message) {
    AddErrorOrWarning("input", line, column, message, "warning", std::clog);
  }

  // implements DescriptorPool::ErrorCollector-------------------------
  void AddError(const std::string& filename, const std::string& element_name,
                const Message* descriptor, ErrorLocation location,
                const std::string& message) {
    AddErrorOrWarning(filename, -1, -1, message, "error", std::cerr);
  }

  void AddWarning(const std::string& filename, const std::string& element_name,
                  const Message* descriptor, ErrorLocation location,
                  const std::string& message) {
    AddErrorOrWarning(filename, -1, -1, message, "warning", std::clog);
  }

  bool FoundErrors() const { return found_errors_; }

 private:
  void AddErrorOrWarning(const std::string& filename, int line, int column,
                         const std::string& message, const std::string& type,
                         std::ostream& out) {
    // Print full path when running under MSVS
    std::string dfile;
    if (format_ == CommandLineInterface::ERROR_FORMAT_MSVS && tree_ != NULL &&
        tree_->VirtualFileToDiskFile(filename, &dfile)) {
      out << dfile;
    } else {
      out << filename;
    }

    // Users typically expect 1-based line/column numbers, so we add 1
    // to each here.
    if (line != -1) {
      // Allow for both GCC- and Visual-Studio-compatible output.
      switch (format_) {
        case CommandLineInterface::ERROR_FORMAT_GCC:
          out << ":" << (line + 1) << ":" << (column + 1);
          break;
        case CommandLineInterface::ERROR_FORMAT_MSVS:
          out << "(" << (line + 1) << ") : " << type
              << " in column=" << (column + 1);
          break;
      }
    }

    if (type == "warning") {
      out << ": warning: " << message << std::endl;
    } else {
      out << ": " << message << std::endl;
    }
  }

  const ErrorFormat format_;
  DiskSourceTree* tree_;
  bool found_errors_;
};

// -------------------------------------------------------------------

// A GeneratorContext implementation that buffers files in memory, then dumps
// them all to disk on demand.
class CommandLineInterface::GeneratorContextImpl : public GeneratorContext {
 public:
  GeneratorContextImpl(const std::vector<const FileDescriptor*>& parsed_files);
  ~GeneratorContextImpl();

  // Write all files in the directory to disk at the given output location,
  // which must end in a '/'.
  bool WriteAllToDisk(const std::string& prefix);

  // Write the contents of this directory to a ZIP-format archive with the
  // given name.
  bool WriteAllToZip(const std::string& filename);

  // Add a boilerplate META-INF/MANIFEST.MF file as required by the Java JAR
  // format, unless one has already been written.
  void AddJarManifest();

  // Get name of all output files.
  void GetOutputFilenames(std::vector<std::string>* output_filenames);

  // implements GeneratorContext --------------------------------------
  io::ZeroCopyOutputStream* Open(const std::string& filename);
  io::ZeroCopyOutputStream* OpenForAppend(const std::string& filename);
  io::ZeroCopyOutputStream* OpenForInsert(const std::string& filename,
                                          const std::string& insertion_point);
  void ListParsedFiles(std::vector<const FileDescriptor*>* output) {
    *output = parsed_files_;
  }

 private:
  friend class MemoryOutputStream;

  // map instead of unordered_map so that files are written in order (good when
  // writing zips).
  std::map<std::string, std::string*> files_;
  const std::vector<const FileDescriptor*>& parsed_files_;
  bool had_error_;
};

class CommandLineInterface::MemoryOutputStream
    : public io::ZeroCopyOutputStream {
 public:
  MemoryOutputStream(GeneratorContextImpl* directory,
                     const std::string& filename, bool append_mode);
  MemoryOutputStream(GeneratorContextImpl* directory,
                     const std::string& filename,
                     const std::string& insertion_point);
  virtual ~MemoryOutputStream();

  // implements ZeroCopyOutputStream ---------------------------------
  virtual bool Next(void** data, int* size) { return inner_->Next(data, size); }
  virtual void BackUp(int count) { inner_->BackUp(count); }
  virtual int64 ByteCount() const { return inner_->ByteCount(); }

 private:
  // Checks to see if "filename_.meta" exists in directory_; if so, fixes the
  // offsets in that GeneratedCodeInfo record to reflect bytes inserted in
  // filename_ at original offset insertion_offset with length insertion_length.
  // We assume that insertions will not occur within any given annotated span
  // of text.
  void UpdateMetadata(size_t insertion_offset, size_t insertion_length);

  // Where to insert the string when it's done.
  GeneratorContextImpl* directory_;
  std::string filename_;
  std::string insertion_point_;

  // The string we're building.
  std::string data_;

  // Whether we should append the output stream to the existing file.
  bool append_mode_;

  // StringOutputStream writing to data_.
  std::unique_ptr<io::StringOutputStream> inner_;
};

// -------------------------------------------------------------------

CommandLineInterface::GeneratorContextImpl::GeneratorContextImpl(
    const std::vector<const FileDescriptor*>& parsed_files)
    : parsed_files_(parsed_files), had_error_(false) {}

CommandLineInterface::GeneratorContextImpl::~GeneratorContextImpl() {
  STLDeleteValues(&files_);
}

bool CommandLineInterface::GeneratorContextImpl::WriteAllToDisk(
    const std::string& prefix) {
  if (had_error_) {
    return false;
  }

  if (!VerifyDirectoryExists(prefix)) {
    return false;
  }

  for (std::map<std::string, std::string*>::const_iterator iter =
           files_.begin();
       iter != files_.end(); ++iter) {
    const std::string& relative_filename = iter->first;
    const char* data = iter->second->data();
    int size = iter->second->size();

    if (!TryCreateParentDirectory(prefix, relative_filename)) {
      return false;
    }
    std::string filename = prefix + relative_filename;

    // Create the output file.
    int file_descriptor;
    do {
      file_descriptor =
          open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    } while (file_descriptor < 0 && errno == EINTR);

    if (file_descriptor < 0) {
      int error = errno;
      std::cerr << filename << ": " << strerror(error);
      return false;
    }

    // Write the file.
    while (size > 0) {
      int write_result;
      do {
        write_result = write(file_descriptor, data, size);
      } while (write_result < 0 && errno == EINTR);

      if (write_result <= 0) {
        // Write error.

        // FIXME(kenton):  According to the man page, if write() returns zero,
        //   there was no error; write() simply did not write anything.  It's
        //   unclear under what circumstances this might happen, but presumably
        //   errno won't be set in this case.  I am confused as to how such an
        //   event should be handled.  For now I'm treating it as an error,
        //   since retrying seems like it could lead to an infinite loop.  I
        //   suspect this never actually happens anyway.

        if (write_result < 0) {
          int error = errno;
          std::cerr << filename << ": write: " << strerror(error);
        } else {
          std::cerr << filename << ": write() returned zero?" << std::endl;
        }
        return false;
      }

      data += write_result;
      size -= write_result;
    }

    if (close(file_descriptor) != 0) {
      int error = errno;
      std::cerr << filename << ": close: " << strerror(error);
      return false;
    }
  }

  return true;
}

bool CommandLineInterface::GeneratorContextImpl::WriteAllToZip(
    const std::string& filename) {
  if (had_error_) {
    return false;
  }

  // Create the output file.
  int file_descriptor;
  do {
    file_descriptor =
        open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
  } while (file_descriptor < 0 && errno == EINTR);

  if (file_descriptor < 0) {
    int error = errno;
    std::cerr << filename << ": " << strerror(error);
    return false;
  }

  // Create the ZipWriter
  io::FileOutputStream stream(file_descriptor);
  ZipWriter zip_writer(&stream);

  for (std::map<std::string, std::string*>::const_iterator iter =
           files_.begin();
       iter != files_.end(); ++iter) {
    zip_writer.Write(iter->first, *iter->second);
  }

  zip_writer.WriteDirectory();

  if (stream.GetErrno() != 0) {
    std::cerr << filename << ": " << strerror(stream.GetErrno()) << std::endl;
  }

  if (!stream.Close()) {
    std::cerr << filename << ": " << strerror(stream.GetErrno()) << std::endl;
  }

  return true;
}

void CommandLineInterface::GeneratorContextImpl::AddJarManifest() {
  std::string** map_slot = &files_["META-INF/MANIFEST.MF"];
  if (*map_slot == NULL) {
    *map_slot = new std::string(
        "Manifest-Version: 1.0\n"
        "Created-By: 1.6.0 (protoc)\n"
        "\n");
  }
}

void CommandLineInterface::GeneratorContextImpl::GetOutputFilenames(
    std::vector<std::string>* output_filenames) {
  for (std::map<std::string, std::string*>::iterator iter = files_.begin();
       iter != files_.end(); ++iter) {
    output_filenames->push_back(iter->first);
  }
}

io::ZeroCopyOutputStream* CommandLineInterface::GeneratorContextImpl::Open(
    const std::string& filename) {
  return new MemoryOutputStream(this, filename, false);
}

io::ZeroCopyOutputStream*
CommandLineInterface::GeneratorContextImpl::OpenForAppend(
    const std::string& filename) {
  return new MemoryOutputStream(this, filename, true);
}

io::ZeroCopyOutputStream*
CommandLineInterface::GeneratorContextImpl::OpenForInsert(
    const std::string& filename, const std::string& insertion_point) {
  return new MemoryOutputStream(this, filename, insertion_point);
}

// -------------------------------------------------------------------

CommandLineInterface::MemoryOutputStream::MemoryOutputStream(
    GeneratorContextImpl* directory, const std::string& filename,
    bool append_mode)
    : directory_(directory),
      filename_(filename),
      append_mode_(append_mode),
      inner_(new io::StringOutputStream(&data_)) {}

CommandLineInterface::MemoryOutputStream::MemoryOutputStream(
    GeneratorContextImpl* directory, const std::string& filename,
    const std::string& insertion_point)
    : directory_(directory),
      filename_(filename),
      insertion_point_(insertion_point),
      inner_(new io::StringOutputStream(&data_)) {}

void CommandLineInterface::MemoryOutputStream::UpdateMetadata(
    size_t insertion_offset, size_t insertion_length) {
  std::map<std::string, std::string*>::iterator meta_file =
      directory_->files_.find(filename_ + ".meta");
  if (meta_file == directory_->files_.end() || !meta_file->second) {
    // No metadata was recorded for this file.
    return;
  }
  std::string* encoded_data = meta_file->second;
  GeneratedCodeInfo metadata;
  bool is_text_format = false;
  if (!metadata.ParseFromString(*encoded_data)) {
    if (!TextFormat::ParseFromString(*encoded_data, &metadata)) {
      // The metadata is invalid.
      std::cerr << filename_
                << ".meta: Could not parse metadata as wire or text format."
                << std::endl;
      return;
    }
    // Generators that use the public plugin interface emit text-format
    // metadata (because in the public plugin protocol, file content must be
    // UTF8-encoded strings).
    is_text_format = true;
  }
  for (int i = 0; i < metadata.annotation_size(); ++i) {
    GeneratedCodeInfo::Annotation* annotation = metadata.mutable_annotation(i);
    if (annotation->begin() >= insertion_offset) {
      annotation->set_begin(annotation->begin() + insertion_length);
      annotation->set_end(annotation->end() + insertion_length);
    }
  }
  if (is_text_format) {
    TextFormat::PrintToString(metadata, encoded_data);
  } else {
    metadata.SerializeToString(encoded_data);
  }
}

CommandLineInterface::MemoryOutputStream::~MemoryOutputStream() {
  // Make sure all data has been written.
  inner_.reset();

  // Insert into the directory.
  std::string** map_slot = &directory_->files_[filename_];

  if (insertion_point_.empty()) {
    // This was just a regular Open().
    if (*map_slot != NULL) {
      if (append_mode_) {
        (*map_slot)->append(data_);
      } else {
        std::cerr << filename_ << ": Tried to write the same file twice."
                  << std::endl;
        directory_->had_error_ = true;
      }
      return;
    }

    *map_slot = new std::string;
    (*map_slot)->swap(data_);
  } else {
    // This was an OpenForInsert().

    // If the data doesn't end with a clean line break, add one.
    if (!data_.empty() && data_[data_.size() - 1] != '\n') {
      data_.push_back('\n');
    }

    // Find the file we are going to insert into.
    if (*map_slot == NULL) {
      std::cerr << filename_
                << ": Tried to insert into file that doesn't exist."
                << std::endl;
      directory_->had_error_ = true;
      return;
    }
    std::string* target = *map_slot;

    // Find the insertion point.
    std::string magic_string =
        strings::Substitute("@@protoc_insertion_point($0)", insertion_point_);
    std::string::size_type pos = target->find(magic_string);

    if (pos == std::string::npos) {
      std::cerr << filename_ << ": insertion point \"" << insertion_point_
                << "\" not found." << std::endl;
      directory_->had_error_ = true;
      return;
    }

    if ((pos > 3) && (target->substr(pos - 3, 2) == "/*")) {
      // Support for inline "/* @@protoc_insertion_point() */"
      pos = pos - 3;
    } else {
      // Seek backwards to the beginning of the line, which is where we will
      // insert the data.  Note that this has the effect of pushing the
      // insertion point down, so the data is inserted before it.  This is
      // intentional because it means that multiple insertions at the same point
      // will end up in the expected order in the final output.
      pos = target->find_last_of('\n', pos);
      if (pos == std::string::npos) {
        // Insertion point is on the first line.
        pos = 0;
      } else {
        // Advance to character after '\n'.
        ++pos;
      }
    }

    // Extract indent.
    std::string indent_(*target, pos,
                        target->find_first_not_of(" \t", pos) - pos);

    if (indent_.empty()) {
      // No indent.  This makes things easier.
      target->insert(pos, data_);
      UpdateMetadata(pos, data_.size());
    } else {
      // Calculate how much space we need.
      int indent_size = 0;
      for (int i = 0; i < data_.size(); i++) {
        if (data_[i] == '\n') indent_size += indent_.size();
      }

      // Make a hole for it.
      target->insert(pos, data_.size() + indent_size, '\0');
      UpdateMetadata(pos, data_.size() + indent_size);

      // Now copy in the data.
      std::string::size_type data_pos = 0;
      char* target_ptr = ::google::protobuf::string_as_array(target) + pos;
      while (data_pos < data_.size()) {
        // Copy indent.
        memcpy(target_ptr, indent_.data(), indent_.size());
        target_ptr += indent_.size();

        // Copy line from data_.
        // We already guaranteed that data_ ends with a newline (above), so this
        // search can't fail.
        std::string::size_type line_length =
            data_.find_first_of('\n', data_pos) + 1 - data_pos;
        memcpy(target_ptr, data_.data() + data_pos, line_length);
        target_ptr += line_length;
        data_pos += line_length;
      }

      GOOGLE_CHECK_EQ(target_ptr,
               ::google::protobuf::string_as_array(target) + pos + data_.size() + indent_size);
    }
  }
}

// ===================================================================

#if defined(_WIN32) && !defined(__CYGWIN__)
const char* const CommandLineInterface::kPathSeparator = ";";
#else
const char* const CommandLineInterface::kPathSeparator = ":";
#endif

CommandLineInterface::CommandLineInterface()
    : mode_(MODE_COMPILE),
      print_mode_(PRINT_NONE),
      error_format_(ERROR_FORMAT_GCC),
      direct_dependencies_explicitly_set_(false),
      direct_dependencies_violation_msg_(
          kDefaultDirectDependenciesViolationMsg),
      imports_in_descriptor_set_(false),
      source_info_in_descriptor_set_(false),
      disallow_services_(false) {
}
CommandLineInterface::~CommandLineInterface() {}

void CommandLineInterface::RegisterGenerator(const std::string& flag_name,
                                             CodeGenerator* generator,
                                             const std::string& help_text) {
  GeneratorInfo info;
  info.flag_name = flag_name;
  info.generator = generator;
  info.help_text = help_text;
  generators_by_flag_name_[flag_name] = info;
}

void CommandLineInterface::RegisterGenerator(
    const std::string& flag_name, const std::string& option_flag_name,
    CodeGenerator* generator, const std::string& help_text) {
  GeneratorInfo info;
  info.flag_name = flag_name;
  info.option_flag_name = option_flag_name;
  info.generator = generator;
  info.help_text = help_text;
  generators_by_flag_name_[flag_name] = info;
  generators_by_option_name_[option_flag_name] = info;
}

void CommandLineInterface::AllowPlugins(const std::string& exe_name_prefix) {
  plugin_prefix_ = exe_name_prefix;
}

int CommandLineInterface::Run(int argc, const char* const argv[]) {
  Clear();
  switch (ParseArguments(argc, argv)) {
    case PARSE_ARGUMENT_DONE_AND_EXIT:
      return 0;
    case PARSE_ARGUMENT_FAIL:
      return 1;
    case PARSE_ARGUMENT_DONE_AND_CONTINUE:
      break;
  }

  std::vector<const FileDescriptor*> parsed_files;
  std::unique_ptr<DiskSourceTree> disk_source_tree;
  std::unique_ptr<ErrorPrinter> error_collector;
  std::unique_ptr<DescriptorPool> descriptor_pool;
  std::unique_ptr<SimpleDescriptorDatabase> descriptor_set_in_database;
  std::unique_ptr<SourceTreeDescriptorDatabase> source_tree_database;

  // Any --descriptor_set_in FileDescriptorSet objects will be used as a
  // fallback to input_files on command line, so create that db first.
  if (!descriptor_set_in_names_.empty()) {
    descriptor_set_in_database.reset(new SimpleDescriptorDatabase());
    if (!PopulateSimpleDescriptorDatabase(descriptor_set_in_database.get())) {
      return 1;
    }
  }

  if (proto_path_.empty()) {
    // If there are no --proto_path flags, then just look in the specified
    // --descriptor_set_in files.  But first, verify that the input files are
    // there.
    if (!VerifyInputFilesInDescriptors(descriptor_set_in_database.get())) {
      return 1;
    }

    error_collector.reset(new ErrorPrinter(error_format_));
    descriptor_pool.reset(new DescriptorPool(descriptor_set_in_database.get(),
                                             error_collector.get()));
  } else {
    disk_source_tree.reset(new DiskSourceTree());
    if (!InitializeDiskSourceTree(disk_source_tree.get(),
                                  descriptor_set_in_database.get())) {
      return 1;
    }

    error_collector.reset(
        new ErrorPrinter(error_format_, disk_source_tree.get()));

    source_tree_database.reset(new SourceTreeDescriptorDatabase(
        disk_source_tree.get(), descriptor_set_in_database.get()));
    source_tree_database->RecordErrorsTo(error_collector.get());

    descriptor_pool.reset(new DescriptorPool(
        source_tree_database.get(),
        source_tree_database->GetValidationErrorCollector()));
  }

  descriptor_pool->EnforceWeakDependencies(true);
  if (!ParseInputFiles(descriptor_pool.get(), &parsed_files)) {
    return 1;
  }


  // We construct a separate GeneratorContext for each output location.  Note
  // that two code generators may output to the same location, in which case
  // they should share a single GeneratorContext so that OpenForInsert() works.
  GeneratorContextMap output_directories;

  // Generate output.
  if (mode_ == MODE_COMPILE) {
    for (int i = 0; i < output_directives_.size(); i++) {
      std::string output_location = output_directives_[i].output_location;
      if (!HasSuffixString(output_location, ".zip") &&
          !HasSuffixString(output_location, ".jar")) {
        AddTrailingSlash(&output_location);
      }
      GeneratorContextImpl** map_slot = &output_directories[output_location];

      if (*map_slot == NULL) {
        // First time we've seen this output location.
        *map_slot = new GeneratorContextImpl(parsed_files);
      }

      if (!GenerateOutput(parsed_files, output_directives_[i], *map_slot)) {
        STLDeleteValues(&output_directories);
        return 1;
      }
    }
  }

  // Write all output to disk.
  for (GeneratorContextMap::iterator iter = output_directories.begin();
       iter != output_directories.end(); ++iter) {
    const std::string& location = iter->first;
    GeneratorContextImpl* directory = iter->second;
    if (HasSuffixString(location, "/")) {
      if (!directory->WriteAllToDisk(location)) {
        STLDeleteValues(&output_directories);
        return 1;
      }
    } else {
      if (HasSuffixString(location, ".jar")) {
        directory->AddJarManifest();
      }

      if (!directory->WriteAllToZip(location)) {
        STLDeleteValues(&output_directories);
        return 1;
      }
    }
  }

  if (!dependency_out_name_.empty()) {
    GOOGLE_DCHECK(disk_source_tree.get());
    if (!GenerateDependencyManifestFile(parsed_files, output_directories,
                                        disk_source_tree.get())) {
      return 1;
    }
  }

  STLDeleteValues(&output_directories);

  if (!descriptor_set_out_name_.empty()) {
    if (!WriteDescriptorSet(parsed_files)) {
      return 1;
    }
  }

  if (mode_ == MODE_ENCODE || mode_ == MODE_DECODE) {
    if (codec_type_.empty()) {
      // HACK:  Define an EmptyMessage type to use for decoding.
      DescriptorPool pool;
      FileDescriptorProto file;
      file.set_name("empty_message.proto");
      file.add_message_type()->set_name("EmptyMessage");
      GOOGLE_CHECK(pool.BuildFile(file) != NULL);
      codec_type_ = "EmptyMessage";
      if (!EncodeOrDecode(&pool)) {
        return 1;
      }
    } else {
      if (!EncodeOrDecode(descriptor_pool.get())) {
        return 1;
      }
    }
  }

  if (error_collector->FoundErrors()) {
    return 1;
  }

  if (mode_ == MODE_PRINT) {
    switch (print_mode_) {
      case PRINT_FREE_FIELDS:
        for (int i = 0; i < parsed_files.size(); ++i) {
          const FileDescriptor* fd = parsed_files[i];
          for (int j = 0; j < fd->message_type_count(); ++j) {
            PrintFreeFieldNumbers(fd->message_type(j));
          }
        }
        break;
      case PRINT_NONE:
        GOOGLE_LOG(ERROR) << "If the code reaches here, it usually means a bug of "
                      "flag parsing in the CommandLineInterface.";
        return 1;

        // Do not add a default case.
    }
  }

  return 0;
}

bool CommandLineInterface::InitializeDiskSourceTree(
    DiskSourceTree* source_tree, DescriptorDatabase* fallback_database) {
  AddDefaultProtoPaths(&proto_path_);

  // Set up the source tree.
  for (int i = 0; i < proto_path_.size(); i++) {
    source_tree->MapPath(proto_path_[i].first, proto_path_[i].second);
  }

  // Map input files to virtual paths if possible.
  if (!MakeInputsBeProtoPathRelative(source_tree, fallback_database)) {
    return false;
  }

  return true;
}

bool CommandLineInterface::PopulateSimpleDescriptorDatabase(
    SimpleDescriptorDatabase* database) {
  for (int i = 0; i < descriptor_set_in_names_.size(); i++) {
    int fd;
    do {
      fd = open(descriptor_set_in_names_[i].c_str(), O_RDONLY | O_BINARY);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
      std::cerr << descriptor_set_in_names_[i] << ": " << strerror(ENOENT)
                << std::endl;
      return false;
    }

    FileDescriptorSet file_descriptor_set;
    bool parsed = file_descriptor_set.ParseFromFileDescriptor(fd);
    if (close(fd) != 0) {
      std::cerr << descriptor_set_in_names_[i] << ": close: " << strerror(errno)
                << std::endl;
      return false;
    }

    if (!parsed) {
      std::cerr << descriptor_set_in_names_[i] << ": Unable to parse."
                << std::endl;
      return false;
    }

    for (int j = 0; j < file_descriptor_set.file_size(); j++) {
      FileDescriptorProto previously_added_file_descriptor_proto;
      if (database->FindFileByName(file_descriptor_set.file(j).name(),
                                   &previously_added_file_descriptor_proto)) {
        // already present - skip
        continue;
      }
      if (!database->Add(file_descriptor_set.file(j))) {
        return false;
      }
    }
  }
  return true;
}

bool CommandLineInterface::VerifyInputFilesInDescriptors(
    DescriptorDatabase* database) {
  for (const auto& input_file : input_files_) {
    FileDescriptorProto file_descriptor;
    if (!database->FindFileByName(input_file, &file_descriptor)) {
      std::cerr << input_file << ": " << strerror(ENOENT) << std::endl;
      return false;
    }

    // Enforce --disallow_services.
    if (disallow_services_ && file_descriptor.service_size() > 0) {
      std::cerr << file_descriptor.name()
                << ": This file contains services, but "
                   "--disallow_services was used."
                << std::endl;
      return false;
    }
  }
  return true;
}

bool CommandLineInterface::ParseInputFiles(
    DescriptorPool* descriptor_pool,
    std::vector<const FileDescriptor*>* parsed_files) {

  // Parse each file.
  for (const auto& input_file : input_files_) {
    // Import the file.
    descriptor_pool->AddUnusedImportTrackFile(input_file);
    const FileDescriptor* parsed_file =
        descriptor_pool->FindFileByName(input_file);
    descriptor_pool->ClearUnusedImportTrackFiles();
    if (parsed_file == NULL) {
      return false;
    }
    parsed_files->push_back(parsed_file);

    // Enforce --disallow_services.
    if (disallow_services_ && parsed_file->service_count() > 0) {
      std::cerr << parsed_file->name()
                << ": This file contains services, but "
                   "--disallow_services was used."
                << std::endl;
      return false;
    }

    // Enforce --direct_dependencies
    if (direct_dependencies_explicitly_set_) {
      bool indirect_imports = false;
      for (int i = 0; i < parsed_file->dependency_count(); i++) {
        if (direct_dependencies_.find(parsed_file->dependency(i)->name()) ==
            direct_dependencies_.end()) {
          indirect_imports = true;
          std::cerr << parsed_file->name() << ": "
                    << StringReplace(direct_dependencies_violation_msg_, "%s",
                                     parsed_file->dependency(i)->name(),
                                     true /* replace_all */)
                    << std::endl;
        }
      }
      if (indirect_imports) {
        return false;
      }
    }
  }
  return true;
}

void CommandLineInterface::Clear() {
  // Clear all members that are set by Run().  Note that we must not clear
  // members which are set by other methods before Run() is called.
  executable_name_.clear();
  proto_path_.clear();
  input_files_.clear();
  direct_dependencies_.clear();
  direct_dependencies_violation_msg_ = kDefaultDirectDependenciesViolationMsg;
  output_directives_.clear();
  codec_type_.clear();
  descriptor_set_in_names_.clear();
  descriptor_set_out_name_.clear();
  dependency_out_name_.clear();


  mode_ = MODE_COMPILE;
  print_mode_ = PRINT_NONE;
  imports_in_descriptor_set_ = false;
  source_info_in_descriptor_set_ = false;
  disallow_services_ = false;
  direct_dependencies_explicitly_set_ = false;
}

bool CommandLineInterface::MakeProtoProtoPathRelative(
    DiskSourceTree* source_tree, std::string* proto,
    DescriptorDatabase* fallback_database) {
  // If it's in the fallback db, don't report non-existent file errors.
  FileDescriptorProto fallback_file;
  bool in_fallback_database =
      fallback_database != nullptr &&
      fallback_database->FindFileByName(*proto, &fallback_file);

  // If the input file path is not a physical file path, it must be a virtual
  // path.
  if (access(proto->c_str(), F_OK) < 0) {
    std::string disk_file;
    if (source_tree->VirtualFileToDiskFile(*proto, &disk_file) ||
        in_fallback_database) {
      return true;
    } else {
      std::cerr << *proto << ": " << strerror(ENOENT) << std::endl;
      return false;
    }
  }

  std::string virtual_file, shadowing_disk_file;
  switch (source_tree->DiskFileToVirtualFile(*proto, &virtual_file,
                                             &shadowing_disk_file)) {
    case DiskSourceTree::SUCCESS:
      *proto = virtual_file;
      break;
    case DiskSourceTree::SHADOWED:
      std::cerr << *proto << ": Input is shadowed in the --proto_path by \""
                << shadowing_disk_file
                << "\".  Either use the latter file as your input or reorder "
                   "the --proto_path so that the former file's location "
                   "comes first."
                << std::endl;
      return false;
    case DiskSourceTree::CANNOT_OPEN:
      if (in_fallback_database) {
        return true;
      }
      std::cerr << *proto << ": " << strerror(errno) << std::endl;
      return false;
    case DiskSourceTree::NO_MAPPING: {
      // Try to interpret the path as a virtual path.
      std::string disk_file;
      if (source_tree->VirtualFileToDiskFile(*proto, &disk_file) ||
          in_fallback_database) {
        return true;
      } else {
        // The input file path can't be mapped to any --proto_path and it also
        // can't be interpreted as a virtual path.
        std::cerr
            << *proto
            << ": File does not reside within any path "
               "specified using --proto_path (or -I).  You must specify a "
               "--proto_path which encompasses this file.  Note that the "
               "proto_path must be an exact prefix of the .proto file "
               "names -- protoc is too dumb to figure out when two paths "
               "(e.g. absolute and relative) are equivalent (it's harder "
               "than you think)."
            << std::endl;
        return false;
      }
    }
  }
  return true;
}

bool CommandLineInterface::MakeInputsBeProtoPathRelative(
    DiskSourceTree* source_tree, DescriptorDatabase* fallback_database) {
  for (auto& input_file : input_files_) {
    if (!MakeProtoProtoPathRelative(source_tree, &input_file,
                                    fallback_database)) {
      return false;
    }
  }

  return true;
}


bool CommandLineInterface::ExpandArgumentFile(
    const std::string& file, std::vector<std::string>* arguments) {
  // The argument file is searched in the working directory only. We don't
  // use the proto import path here.
  std::ifstream file_stream(file.c_str());
  if (!file_stream.is_open()) {
    return false;
  }
  std::string argument;
  // We don't support any kind of shell expansion right now.
  while (std::getline(file_stream, argument)) {
    arguments->push_back(argument);
  }
  return true;
}

CommandLineInterface::ParseArgumentStatus CommandLineInterface::ParseArguments(
    int argc, const char* const argv[]) {
  executable_name_ = argv[0];

  std::vector<std::string> arguments;
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '@') {
      if (!ExpandArgumentFile(argv[i] + 1, &arguments)) {
        std::cerr << "Failed to open argument file: " << (argv[i] + 1)
                  << std::endl;
        return PARSE_ARGUMENT_FAIL;
      }
      continue;
    }
    arguments.push_back(argv[i]);
  }

  // if no arguments are given, show help
  if (arguments.empty()) {
    PrintHelpText();
    return PARSE_ARGUMENT_DONE_AND_EXIT;  // Exit without running compiler.
  }

  // Iterate through all arguments and parse them.
  for (int i = 0; i < arguments.size(); ++i) {
    std::string name, value;

    if (ParseArgument(arguments[i].c_str(), &name, &value)) {
      // Returned true => Use the next argument as the flag value.
      if (i + 1 == arguments.size() || arguments[i + 1][0] == '-') {
        std::cerr << "Missing value for flag: " << name << std::endl;
        if (name == "--decode") {
          std::cerr << "To decode an unknown message, use --decode_raw."
                    << std::endl;
        }
        return PARSE_ARGUMENT_FAIL;
      } else {
        ++i;
        value = arguments[i];
      }
    }

    ParseArgumentStatus status = InterpretArgument(name, value);
    if (status != PARSE_ARGUMENT_DONE_AND_CONTINUE) return status;
  }

  // Make sure each plugin option has a matching plugin output.
  bool foundUnknownPluginOption = false;
  for (std::map<std::string, std::string>::const_iterator i =
           plugin_parameters_.begin();
       i != plugin_parameters_.end(); ++i) {
    if (plugins_.find(i->first) != plugins_.end()) {
      continue;
    }
    bool foundImplicitPlugin = false;
    for (std::vector<OutputDirective>::const_iterator j =
             output_directives_.begin();
         j != output_directives_.end(); ++j) {
      if (j->generator == NULL) {
        std::string plugin_name = PluginName(plugin_prefix_, j->name);
        if (plugin_name == i->first) {
          foundImplicitPlugin = true;
          break;
        }
      }
    }
    if (!foundImplicitPlugin) {
      std::cerr << "Unknown flag: "
                // strip prefix + "gen-" and add back "_opt"
                << "--" + i->first.substr(plugin_prefix_.size() + 4) + "_opt"
                << std::endl;
      foundUnknownPluginOption = true;
    }
  }
  if (foundUnknownPluginOption) {
    return PARSE_ARGUMENT_FAIL;
  }

  // The --proto_path & --descriptor_set_in flags both specify places to look
  // for proto files. If neither were given, use the current working directory.
  if (proto_path_.empty() && descriptor_set_in_names_.empty()) {
    // Don't use make_pair as the old/default standard library on Solaris
    // doesn't support it without explicit template parameters, which are
    // incompatible with C++0x's make_pair.
    proto_path_.push_back(std::pair<std::string, std::string>("", "."));
  }

  // Check some error cases.
  bool decoding_raw = (mode_ == MODE_DECODE) && codec_type_.empty();
  if (decoding_raw && !input_files_.empty()) {
    std::cerr << "When using --decode_raw, no input files should be given."
              << std::endl;
    return PARSE_ARGUMENT_FAIL;
  } else if (!decoding_raw && input_files_.empty()) {
    std::cerr << "Missing input file." << std::endl;
    return PARSE_ARGUMENT_FAIL;
  }
  if (mode_ == MODE_COMPILE && output_directives_.empty() &&
      descriptor_set_out_name_.empty()) {
    std::cerr << "Missing output directives." << std::endl;
    return PARSE_ARGUMENT_FAIL;
  }
  if (mode_ != MODE_COMPILE && !dependency_out_name_.empty()) {
    std::cerr << "Can only use --dependency_out=FILE when generating code."
              << std::endl;
    return PARSE_ARGUMENT_FAIL;
  }
  if (!dependency_out_name_.empty() && input_files_.size() > 1) {
    std::cerr
        << "Can only process one input file when using --dependency_out=FILE."
        << std::endl;
    return PARSE_ARGUMENT_FAIL;
  }
  if (imports_in_descriptor_set_ && descriptor_set_out_name_.empty()) {
    std::cerr << "--include_imports only makes sense when combined with "
                 "--descriptor_set_out."
              << std::endl;
  }
  if (source_info_in_descriptor_set_ && descriptor_set_out_name_.empty()) {
    std::cerr << "--include_source_info only makes sense when combined with "
                 "--descriptor_set_out."
              << std::endl;
  }

  return PARSE_ARGUMENT_DONE_AND_CONTINUE;
}

bool CommandLineInterface::ParseArgument(const char* arg, std::string* name,
                                         std::string* value) {
  bool parsed_value = false;

  if (arg[0] != '-') {
    // Not a flag.
    name->clear();
    parsed_value = true;
    *value = arg;
  } else if (arg[1] == '-') {
    // Two dashes:  Multi-character name, with '=' separating name and
    //   value.
    const char* equals_pos = strchr(arg, '=');
    if (equals_pos != NULL) {
      *name = std::string(arg, equals_pos - arg);
      *value = equals_pos + 1;
      parsed_value = true;
    } else {
      *name = arg;
    }
  } else {
    // One dash:  One-character name, all subsequent characters are the
    //   value.
    if (arg[1] == '\0') {
      // arg is just "-".  We treat this as an input file, except that at
      // present this will just lead to a "file not found" error.
      name->clear();
      *value = arg;
      parsed_value = true;
    } else {
      *name = std::string(arg, 2);
      *value = arg + 2;
      parsed_value = !value->empty();
    }
  }

  // Need to return true iff the next arg should be used as the value for this
  // one, false otherwise.

  if (parsed_value) {
    // We already parsed a value for this flag.
    return false;
  }

  if (*name == "-h" || *name == "--help" || *name == "--disallow_services" ||
      *name == "--include_imports" || *name == "--include_source_info" ||
      *name == "--version" || *name == "--decode_raw" ||
      *name == "--print_free_field_numbers") {
    // HACK:  These are the only flags that don't take a value.
    //   They probably should not be hard-coded like this but for now it's
    //   not worth doing better.
    return false;
  }

  // Next argument is the flag value.
  return true;
}

CommandLineInterface::ParseArgumentStatus
CommandLineInterface::InterpretArgument(const std::string& name,
                                        const std::string& value) {
  if (name.empty()) {
    // Not a flag.  Just a filename.
    if (value.empty()) {
      std::cerr
          << "You seem to have passed an empty string as one of the "
             "arguments to "
          << executable_name_
          << ".  This is actually "
             "sort of hard to do.  Congrats.  Unfortunately it is not valid "
             "input so the program is going to die now."
          << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }

    input_files_.push_back(value);

  } else if (name == "-I" || name == "--proto_path") {
    // Java's -classpath (and some other languages) delimits path components
    // with colons.  Let's accept that syntax too just to make things more
    // intuitive.
    std::vector<std::string> parts = Split(
        value, CommandLineInterface::kPathSeparator,
        true);

    for (int i = 0; i < parts.size(); i++) {
      std::string virtual_path;
      std::string disk_path;

      std::string::size_type equals_pos = parts[i].find_first_of('=');
      if (equals_pos == std::string::npos) {
        virtual_path = "";
        disk_path = parts[i];
      } else {
        virtual_path = parts[i].substr(0, equals_pos);
        disk_path = parts[i].substr(equals_pos + 1);
      }

      if (disk_path.empty()) {
        std::cerr
            << "--proto_path passed empty directory name.  (Use \".\" for "
               "current directory.)"
            << std::endl;
        return PARSE_ARGUMENT_FAIL;
      }

      // Make sure disk path exists, warn otherwise.
      if (access(disk_path.c_str(), F_OK) < 0) {
        // Try the original path; it may have just happened to have a '=' in it.
        if (access(parts[i].c_str(), F_OK) < 0) {
          std::cerr << disk_path << ": warning: directory does not exist."
                    << std::endl;
        } else {
          virtual_path = "";
          disk_path = parts[i];
        }
      }

      // Don't use make_pair as the old/default standard library on Solaris
      // doesn't support it without explicit template parameters, which are
      // incompatible with C++0x's make_pair.
      proto_path_.push_back(
          std::pair<std::string, std::string>(virtual_path, disk_path));
    }

  } else if (name == "--direct_dependencies") {
    if (direct_dependencies_explicitly_set_) {
      std::cerr << name
                << " may only be passed once. To specify multiple "
                   "direct dependencies, pass them all as a single "
                   "parameter separated by ':'."
                << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }

    direct_dependencies_explicitly_set_ = true;
    std::vector<std::string> direct =
        Split(value, ":", true);
    GOOGLE_DCHECK(direct_dependencies_.empty());
    direct_dependencies_.insert(direct.begin(), direct.end());

  } else if (name == "--direct_dependencies_violation_msg") {
    direct_dependencies_violation_msg_ = value;

  } else if (name == "--descriptor_set_in") {
    if (!descriptor_set_in_names_.empty()) {
      std::cerr << name
                << " may only be passed once. To specify multiple "
                   "descriptor sets, pass them all as a single "
                   "parameter separated by '"
                << CommandLineInterface::kPathSeparator << "'." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    if (value.empty()) {
      std::cerr << name << " requires a non-empty value." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    if (!dependency_out_name_.empty()) {
      std::cerr << name << " cannot be used with --dependency_out."
                << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }

    descriptor_set_in_names_ = Split(
        value, CommandLineInterface::kPathSeparator,
        true);

  } else if (name == "-o" || name == "--descriptor_set_out") {
    if (!descriptor_set_out_name_.empty()) {
      std::cerr << name << " may only be passed once." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    if (value.empty()) {
      std::cerr << name << " requires a non-empty value." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    if (mode_ != MODE_COMPILE) {
      std::cerr
          << "Cannot use --encode or --decode and generate descriptors at the "
             "same time."
          << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    descriptor_set_out_name_ = value;

  } else if (name == "--dependency_out") {
    if (!dependency_out_name_.empty()) {
      std::cerr << name << " may only be passed once." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    if (value.empty()) {
      std::cerr << name << " requires a non-empty value." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    if (!descriptor_set_in_names_.empty()) {
      std::cerr << name << " cannot be used with --descriptor_set_in."
                << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    dependency_out_name_ = value;

  } else if (name == "--include_imports") {
    if (imports_in_descriptor_set_) {
      std::cerr << name << " may only be passed once." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    imports_in_descriptor_set_ = true;

  } else if (name == "--include_source_info") {
    if (source_info_in_descriptor_set_) {
      std::cerr << name << " may only be passed once." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    source_info_in_descriptor_set_ = true;

  } else if (name == "-h" || name == "--help") {
    PrintHelpText();
    return PARSE_ARGUMENT_DONE_AND_EXIT;  // Exit without running compiler.

  } else if (name == "--version") {
    if (!version_info_.empty()) {
      std::cout << version_info_ << std::endl;
    }
    std::cout << "libprotoc " << internal::VersionString(PROTOBUF_VERSION)
              << std::endl;
    return PARSE_ARGUMENT_DONE_AND_EXIT;  // Exit without running compiler.

  } else if (name == "--disallow_services") {
    disallow_services_ = true;

  } else if (name == "--encode" || name == "--decode" ||
             name == "--decode_raw") {
    if (mode_ != MODE_COMPILE) {
      std::cerr << "Only one of --encode and --decode can be specified."
                << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    if (!output_directives_.empty() || !descriptor_set_out_name_.empty()) {
      std::cerr << "Cannot use " << name
                << " and generate code or descriptors at the same time."
                << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }

    mode_ = (name == "--encode") ? MODE_ENCODE : MODE_DECODE;

    if (value.empty() && name != "--decode_raw") {
      std::cerr << "Type name for " << name << " cannot be blank." << std::endl;
      if (name == "--decode") {
        std::cerr << "To decode an unknown message, use --decode_raw."
                  << std::endl;
      }
      return PARSE_ARGUMENT_FAIL;
    } else if (!value.empty() && name == "--decode_raw") {
      std::cerr << "--decode_raw does not take a parameter." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }

    codec_type_ = value;

  } else if (name == "--error_format") {
    if (value == "gcc") {
      error_format_ = ERROR_FORMAT_GCC;
    } else if (value == "msvs") {
      error_format_ = ERROR_FORMAT_MSVS;
    } else {
      std::cerr << "Unknown error format: " << value << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }

  } else if (name == "--plugin") {
    if (plugin_prefix_.empty()) {
      std::cerr << "This compiler does not support plugins." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }

    std::string plugin_name;
    std::string path;

    std::string::size_type equals_pos = value.find_first_of('=');
    if (equals_pos == std::string::npos) {
      // Use the basename of the file.
      std::string::size_type slash_pos = value.find_last_of('/');
      if (slash_pos == std::string::npos) {
        plugin_name = value;
      } else {
        plugin_name = value.substr(slash_pos + 1);
      }
      path = value;
    } else {
      plugin_name = value.substr(0, equals_pos);
      path = value.substr(equals_pos + 1);
    }

    plugins_[plugin_name] = path;

  } else if (name == "--print_free_field_numbers") {
    if (mode_ != MODE_COMPILE) {
      std::cerr << "Cannot use " << name
                << " and use --encode, --decode or print "
                << "other info at the same time." << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    if (!output_directives_.empty() || !descriptor_set_out_name_.empty()) {
      std::cerr << "Cannot use " << name
                << " and generate code or descriptors at the same time."
                << std::endl;
      return PARSE_ARGUMENT_FAIL;
    }
    mode_ = MODE_PRINT;
    print_mode_ = PRINT_FREE_FIELDS;
  } else {
    // Some other flag.  Look it up in the generators list.
    const GeneratorInfo* generator_info =
        FindOrNull(generators_by_flag_name_, name);
    if (generator_info == NULL &&
        (plugin_prefix_.empty() || !HasSuffixString(name, "_out"))) {
      // Check if it's a generator option flag.
      generator_info = FindOrNull(generators_by_option_name_, name);
      if (generator_info != NULL) {
        std::string* parameters =
            &generator_parameters_[generator_info->flag_name];
        if (!parameters->empty()) {
          parameters->append(",");
        }
        parameters->append(value);
      } else if (HasPrefixString(name, "--") && HasSuffixString(name, "_opt")) {
        std::string* parameters =
            &plugin_parameters_[PluginName(plugin_prefix_, name)];
        if (!parameters->empty()) {
          parameters->append(",");
        }
        parameters->append(value);
      } else {
        std::cerr << "Unknown flag: " << name << std::endl;
        return PARSE_ARGUMENT_FAIL;
      }
    } else {
      // It's an output flag.  Add it to the output directives.
      if (mode_ != MODE_COMPILE) {
        std::cerr << "Cannot use --encode, --decode or print .proto info and "
                     "generate code at the same time."
                  << std::endl;
        return PARSE_ARGUMENT_FAIL;
      }

      OutputDirective directive;
      directive.name = name;
      if (generator_info == NULL) {
        directive.generator = NULL;
      } else {
        directive.generator = generator_info->generator;
      }

      // Split value at ':' to separate the generator parameter from the
      // filename.  However, avoid doing this if the colon is part of a valid
      // Windows-style absolute path.
      std::string::size_type colon_pos = value.find_first_of(':');
      if (colon_pos == std::string::npos || IsWindowsAbsolutePath(value)) {
        directive.output_location = value;
      } else {
        directive.parameter = value.substr(0, colon_pos);
        directive.output_location = value.substr(colon_pos + 1);
      }

      output_directives_.push_back(directive);
    }
  }

  return PARSE_ARGUMENT_DONE_AND_CONTINUE;
}

void CommandLineInterface::PrintHelpText() {
  // Sorry for indentation here; line wrapping would be uglier.
  std::cout
      <<
      "Usage: " << executable_name_
      << " [OPTION] PROTO_FILES\n"
         "Parse PROTO_FILES and generate output based on the options given:\n"
         "  -IPATH, --proto_path=PATH   Specify the directory in which to "
         "search for\n"
         "                              imports.  May be specified multiple "
         "times;\n"
         "                              directories will be searched in order. "
         " If not\n"
         "                              given, the current working directory "
         "is used.\n"
         "                              If not found in any of the these "
         "directories,\n"
         "                              the --descriptor_set_in descriptors "
         "will be\n"
         "                              checked for required proto file.\n"
         "  --version                   Show version info and exit.\n"
         "  -h, --help                  Show this text and exit.\n"
         "  --encode=MESSAGE_TYPE       Read a text-format message of the "
         "given type\n"
         "                              from standard input and write it in "
         "binary\n"
         "                              to standard output.  The message type "
         "must\n"
         "                              be defined in PROTO_FILES or their "
         "imports.\n"
         "  --decode=MESSAGE_TYPE       Read a binary message of the given "
         "type from\n"
         "                              standard input and write it in text "
         "format\n"
         "                              to standard output.  The message type "
         "must\n"
         "                              be defined in PROTO_FILES or their "
         "imports.\n"
         "  --decode_raw                Read an arbitrary protocol message "
         "from\n"
         "                              standard input and write the raw "
         "tag/value\n"
         "                              pairs in text format to standard "
         "output.  No\n"
         "                              PROTO_FILES should be given when using "
         "this\n"
         "                              flag.\n"
         "  --descriptor_set_in=FILES   Specifies a delimited list of FILES\n"
         "                              each containing a FileDescriptorSet "
         "(a\n"
         "                              protocol buffer defined in "
         "descriptor.proto).\n"
         "                              The FileDescriptor for each of the "
         "PROTO_FILES\n"
         "                              provided will be loaded from these\n"
         "                              FileDescriptorSets. If a "
         "FileDescriptor\n"
         "                              appears multiple times, the first "
         "occurrence\n"
         "                              will be used.\n"
         "  -oFILE,                     Writes a FileDescriptorSet (a protocol "
         "buffer,\n"
         "    --descriptor_set_out=FILE defined in descriptor.proto) "
         "containing all of\n"
         "                              the input files to FILE.\n"
         "  --include_imports           When using --descriptor_set_out, also "
         "include\n"
         "                              all dependencies of the input files in "
         "the\n"
         "                              set, so that the set is "
         "self-contained.\n"
         "  --include_source_info       When using --descriptor_set_out, do "
         "not strip\n"
         "                              SourceCodeInfo from the "
         "FileDescriptorProto.\n"
         "                              This results in vastly larger "
         "descriptors that\n"
         "                              include information about the "
         "original\n"
         "                              location of each decl in the source "
         "file as\n"
         "                              well as surrounding comments.\n"
         "  --dependency_out=FILE       Write a dependency output file in the "
         "format\n"
         "                              expected by make. This writes the "
         "transitive\n"
         "                              set of input file paths to FILE\n"
         "  --error_format=FORMAT       Set the format in which to print "
         "errors.\n"
         "                              FORMAT may be 'gcc' (the default) or "
         "'msvs'\n"
         "                              (Microsoft Visual Studio format).\n"
         "  --print_free_field_numbers  Print the free field numbers of the "
         "messages\n"
         "                              defined in the given proto files. "
         "Groups share\n"
         "                              the same field number space with the "
         "parent \n"
         "                              message. Extension ranges are counted "
         "as \n"
         "                              occupied fields numbers.\n"
      << std::endl;
  if (!plugin_prefix_.empty()) {
    std::cout
        << "  --plugin=EXECUTABLE         Specifies a plugin executable to "
           "use.\n"
           "                              Normally, protoc searches the PATH "
           "for\n"
           "                              plugins, but you may specify "
           "additional\n"
           "                              executables not in the path using "
           "this flag.\n"
           "                              Additionally, EXECUTABLE may be of "
           "the form\n"
           "                              NAME=PATH, in which case the given "
           "plugin name\n"
           "                              is mapped to the given executable "
           "even if\n"
           "                              the executable's own name differs."
        << std::endl;
  }

  for (GeneratorMap::iterator iter = generators_by_flag_name_.begin();
       iter != generators_by_flag_name_.end(); ++iter) {
    // FIXME(kenton):  If the text is long enough it will wrap, which is ugly,
    //   but fixing this nicely (e.g. splitting on spaces) is probably more
    //   trouble than it's worth.
    std::cout << "  " << iter->first << "=OUT_DIR "
              << std::string(19 - iter->first.size(),
                             ' ')  // Spaces for alignment.
              << iter->second.help_text << std::endl;
  }
  std::cout << "  @<filename>                 Read options and filenames from "
               "file. If a\n"
               "                              relative file path is specified, "
               "the file\n"
               "                              will be searched in the working "
               "directory.\n"
               "                              The --proto_path option will not "
               "affect how\n"
               "                              this argument file is searched. "
               "Content of\n"
               "                              the file will be expanded in the "
               "position of\n"
               "                              @<filename> as in the argument "
               "list. Note\n"
               "                              that shell expansion is not "
               "applied to the\n"
               "                              content of the file (i.e., you "
               "cannot use\n"
               "                              quotes, wildcards, escapes, "
               "commands, etc.).\n"
               "                              Each line corresponds to a "
               "single argument,\n"
               "                              even if it contains spaces."
            << std::endl;
}

bool CommandLineInterface::GenerateOutput(
    const std::vector<const FileDescriptor*>& parsed_files,
    const OutputDirective& output_directive,
    GeneratorContext* generator_context) {
  // Call the generator.
  std::string error;
  if (output_directive.generator == NULL) {
    // This is a plugin.
    GOOGLE_CHECK(HasPrefixString(output_directive.name, "--") &&
          HasSuffixString(output_directive.name, "_out"))
        << "Bad name for plugin generator: " << output_directive.name;

    std::string plugin_name = PluginName(plugin_prefix_, output_directive.name);
    std::string parameters = output_directive.parameter;
    if (!plugin_parameters_[plugin_name].empty()) {
      if (!parameters.empty()) {
        parameters.append(",");
      }
      parameters.append(plugin_parameters_[plugin_name]);
    }
    if (!GeneratePluginOutput(parsed_files, plugin_name, parameters,
                              generator_context, &error)) {
      std::cerr << output_directive.name << ": " << error << std::endl;
      return false;
    }
  } else {
    // Regular generator.
    std::string parameters = output_directive.parameter;
    if (!generator_parameters_[output_directive.name].empty()) {
      if (!parameters.empty()) {
        parameters.append(",");
      }
      parameters.append(generator_parameters_[output_directive.name]);
    }
    if (!output_directive.generator->GenerateAll(parsed_files, parameters,
                                                 generator_context, &error)) {
      // Generator returned an error.
      std::cerr << output_directive.name << ": " << error << std::endl;
      return false;
    }
  }

  return true;
}

bool CommandLineInterface::GenerateDependencyManifestFile(
    const std::vector<const FileDescriptor*>& parsed_files,
    const GeneratorContextMap& output_directories,
    DiskSourceTree* source_tree) {
  FileDescriptorSet file_set;

  std::set<const FileDescriptor*> already_seen;
  for (int i = 0; i < parsed_files.size(); i++) {
    GetTransitiveDependencies(parsed_files[i], false, false, &already_seen,
                              file_set.mutable_file());
  }

  std::vector<std::string> output_filenames;
  for (GeneratorContextMap::const_iterator iter = output_directories.begin();
       iter != output_directories.end(); ++iter) {
    const std::string& location = iter->first;
    GeneratorContextImpl* directory = iter->second;
    std::vector<std::string> relative_output_filenames;
    directory->GetOutputFilenames(&relative_output_filenames);
    for (int i = 0; i < relative_output_filenames.size(); i++) {
      std::string output_filename = location + relative_output_filenames[i];
      if (output_filename.compare(0, 2, "./") == 0) {
        output_filename = output_filename.substr(2);
      }
      output_filenames.push_back(output_filename);
    }
  }

  int fd;
  do {
    fd = open(dependency_out_name_.c_str(),
              O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
  } while (fd < 0 && errno == EINTR);

  if (fd < 0) {
    perror(dependency_out_name_.c_str());
    return false;
  }

  io::FileOutputStream out(fd);
  io::Printer printer(&out, '$');

  for (int i = 0; i < output_filenames.size(); i++) {
    printer.Print(output_filenames[i].c_str());
    if (i == output_filenames.size() - 1) {
      printer.Print(":");
    } else {
      printer.Print(" \\\n");
    }
  }

  for (int i = 0; i < file_set.file_size(); i++) {
    const FileDescriptorProto& file = file_set.file(i);
    const std::string& virtual_file = file.name();
    std::string disk_file;
    if (source_tree &&
        source_tree->VirtualFileToDiskFile(virtual_file, &disk_file)) {
      printer.Print(" $disk_file$", "disk_file", disk_file);
      if (i < file_set.file_size() - 1) printer.Print("\\\n");
    } else {
      std::cerr << "Unable to identify path for file " << virtual_file
                << std::endl;
      return false;
    }
  }

  return true;
}

bool CommandLineInterface::GeneratePluginOutput(
    const std::vector<const FileDescriptor*>& parsed_files,
    const std::string& plugin_name, const std::string& parameter,
    GeneratorContext* generator_context, std::string* error) {
  CodeGeneratorRequest request;
  CodeGeneratorResponse response;
  std::string processed_parameter = parameter;


  // Build the request.
  if (!processed_parameter.empty()) {
    request.set_parameter(processed_parameter);
  }


  std::set<const FileDescriptor*> already_seen;
  for (int i = 0; i < parsed_files.size(); i++) {
    request.add_file_to_generate(parsed_files[i]->name());
    GetTransitiveDependencies(parsed_files[i],
                              true,  // Include json_name for plugins.
                              true,  // Include source code info.
                              &already_seen, request.mutable_proto_file());
  }

  google::protobuf::compiler::Version* version =
      request.mutable_compiler_version();
  version->set_major(PROTOBUF_VERSION / 1000000);
  version->set_minor(PROTOBUF_VERSION / 1000 % 1000);
  version->set_patch(PROTOBUF_VERSION % 1000);
  version->set_suffix(PROTOBUF_VERSION_SUFFIX);

  // Invoke the plugin.
  Subprocess subprocess;

  if (plugins_.count(plugin_name) > 0) {
    subprocess.Start(plugins_[plugin_name], Subprocess::EXACT_NAME);
  } else {
    subprocess.Start(plugin_name, Subprocess::SEARCH_PATH);
  }

  std::string communicate_error;
  if (!subprocess.Communicate(request, &response, &communicate_error)) {
    *error = strings::Substitute("$0: $1", plugin_name, communicate_error);
    return false;
  }

  // Write the files.  We do this even if there was a generator error in order
  // to match the behavior of a compiled-in generator.
  std::unique_ptr<io::ZeroCopyOutputStream> current_output;
  for (int i = 0; i < response.file_size(); i++) {
    const CodeGeneratorResponse::File& output_file = response.file(i);

    if (!output_file.insertion_point().empty()) {
      std::string filename = output_file.name();
      // Open a file for insert.
      // We reset current_output to NULL first so that the old file is closed
      // before the new one is opened.
      current_output.reset();
      current_output.reset(generator_context->OpenForInsert(
          filename, output_file.insertion_point()));
    } else if (!output_file.name().empty()) {
      // Starting a new file.  Open it.
      // We reset current_output to NULL first so that the old file is closed
      // before the new one is opened.
      current_output.reset();
      current_output.reset(generator_context->Open(output_file.name()));
    } else if (current_output == NULL) {
      *error = strings::Substitute(
          "$0: First file chunk returned by plugin did not specify a file "
          "name.",
          plugin_name);
      return false;
    }

    // Use CodedOutputStream for convenience; otherwise we'd need to provide
    // our own buffer-copying loop.
    io::CodedOutputStream writer(current_output.get());
    writer.WriteString(output_file.content());
  }

  // Check for errors.
  if (!response.error().empty()) {
    // Generator returned an error.
    *error = response.error();
    return false;
  }

  return true;
}

bool CommandLineInterface::EncodeOrDecode(const DescriptorPool* pool) {
  // Look up the type.
  const Descriptor* type = pool->FindMessageTypeByName(codec_type_);
  if (type == NULL) {
    std::cerr << "Type not defined: " << codec_type_ << std::endl;
    return false;
  }

  DynamicMessageFactory dynamic_factory(pool);
  std::unique_ptr<Message> message(dynamic_factory.GetPrototype(type)->New());

  if (mode_ == MODE_ENCODE) {
    SetFdToTextMode(STDIN_FILENO);
    SetFdToBinaryMode(STDOUT_FILENO);
  } else {
    SetFdToBinaryMode(STDIN_FILENO);
    SetFdToTextMode(STDOUT_FILENO);
  }

  io::FileInputStream in(STDIN_FILENO);
  io::FileOutputStream out(STDOUT_FILENO);

  if (mode_ == MODE_ENCODE) {
    // Input is text.
    ErrorPrinter error_collector(error_format_);
    TextFormat::Parser parser;
    parser.RecordErrorsTo(&error_collector);
    parser.AllowPartialMessage(true);

    if (!parser.Parse(&in, message.get())) {
      std::cerr << "Failed to parse input." << std::endl;
      return false;
    }
  } else {
    // Input is binary.
    if (!message->ParsePartialFromZeroCopyStream(&in)) {
      std::cerr << "Failed to parse input." << std::endl;
      return false;
    }
  }

  if (!message->IsInitialized()) {
    std::cerr << "warning:  Input message is missing required fields:  "
              << message->InitializationErrorString() << std::endl;
  }

  if (mode_ == MODE_ENCODE) {
    // Output is binary.
    if (!message->SerializePartialToZeroCopyStream(&out)) {
      std::cerr << "output: I/O error." << std::endl;
      return false;
    }
  } else {
    // Output is text.
    if (!TextFormat::Print(*message, &out)) {
      std::cerr << "output: I/O error." << std::endl;
      return false;
    }
  }

  return true;
}

bool CommandLineInterface::WriteDescriptorSet(
    const std::vector<const FileDescriptor*>& parsed_files) {
  FileDescriptorSet file_set;

  std::set<const FileDescriptor*> already_seen;
  if (!imports_in_descriptor_set_) {
    // Since we don't want to output transitive dependencies, but we do want
    // things to be in dependency order, add all dependencies that aren't in
    // parsed_files to already_seen.  This will short circuit the recursion
    // in GetTransitiveDependencies.
    std::set<const FileDescriptor*> to_output;
    to_output.insert(parsed_files.begin(), parsed_files.end());
    for (int i = 0; i < parsed_files.size(); i++) {
      const FileDescriptor* file = parsed_files[i];
      for (int i = 0; i < file->dependency_count(); i++) {
        const FileDescriptor* dependency = file->dependency(i);
        // if the dependency isn't in parsed files, mark it as already seen
        if (to_output.find(dependency) == to_output.end()) {
          already_seen.insert(dependency);
        }
      }
    }
  }
  for (int i = 0; i < parsed_files.size(); i++) {
    GetTransitiveDependencies(parsed_files[i],
                              true,  // Include json_name
                              source_info_in_descriptor_set_, &already_seen,
                              file_set.mutable_file());
  }

  int fd;
  do {
    fd = open(descriptor_set_out_name_.c_str(),
              O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
  } while (fd < 0 && errno == EINTR);

  if (fd < 0) {
    perror(descriptor_set_out_name_.c_str());
    return false;
  }

  io::FileOutputStream out(fd);
  if (!file_set.SerializeToZeroCopyStream(&out)) {
    std::cerr << descriptor_set_out_name_ << ": " << strerror(out.GetErrno())
              << std::endl;
    out.Close();
    return false;
  }
  if (!out.Close()) {
    std::cerr << descriptor_set_out_name_ << ": " << strerror(out.GetErrno())
              << std::endl;
    return false;
  }

  return true;
}

void CommandLineInterface::GetTransitiveDependencies(
    const FileDescriptor* file, bool include_json_name,
    bool include_source_code_info,
    std::set<const FileDescriptor*>* already_seen,
    RepeatedPtrField<FileDescriptorProto>* output) {
  if (!already_seen->insert(file).second) {
    // Already saw this file.  Skip.
    return;
  }

  // Add all dependencies.
  for (int i = 0; i < file->dependency_count(); i++) {
    GetTransitiveDependencies(file->dependency(i), include_json_name,
                              include_source_code_info, already_seen, output);
  }

  // Add this file.
  FileDescriptorProto* new_descriptor = output->Add();
  file->CopyTo(new_descriptor);
  if (include_json_name) {
    file->CopyJsonNameTo(new_descriptor);
  }
  if (include_source_code_info) {
    file->CopySourceCodeInfoTo(new_descriptor);
  }
}

namespace {

// Utility function for PrintFreeFieldNumbers.
// Stores occupied ranges into the ranges parameter, and next level of sub
// message types into the nested_messages parameter.  The FieldRange is left
// inclusive, right exclusive. i.e. [a, b).
//
// Nested Messages:
// Note that it only stores the nested message type, iff the nested type is
// either a direct child of the given descriptor, or the nested type is a
// decendent of the given descriptor and all the nodes between the
// nested type and the given descriptor are group types. e.g.
//
// message Foo {
//   message Bar {
//     message NestedBar {}
//   }
//   group Baz = 1 {
//     group NestedBazGroup = 2 {
//       message Quz {
//         message NestedQuz {}
//       }
//     }
//     message NestedBaz {}
//   }
// }
//
// In this case, Bar, Quz and NestedBaz will be added into the nested types.
// Since free field numbers of group types will not be printed, this makes sure
// the nested message types in groups will not be dropped. The nested_messages
// parameter will contain the direct children (when groups are ignored in the
// tree) of the given descriptor for the caller to traverse. The declaration
// order of the nested messages is also preserved.
typedef std::pair<int, int> FieldRange;
void GatherOccupiedFieldRanges(
    const Descriptor* descriptor, std::set<FieldRange>* ranges,
    std::vector<const Descriptor*>* nested_messages) {
  std::set<const Descriptor*> groups;
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const FieldDescriptor* fd = descriptor->field(i);
    ranges->insert(FieldRange(fd->number(), fd->number() + 1));
    if (fd->type() == FieldDescriptor::TYPE_GROUP) {
      groups.insert(fd->message_type());
    }
  }
  for (int i = 0; i < descriptor->extension_range_count(); ++i) {
    ranges->insert(FieldRange(descriptor->extension_range(i)->start,
                              descriptor->extension_range(i)->end));
  }
  for (int i = 0; i < descriptor->reserved_range_count(); ++i) {
    ranges->insert(FieldRange(descriptor->reserved_range(i)->start,
                              descriptor->reserved_range(i)->end));
  }
  // Handle the nested messages/groups in declaration order to make it
  // post-order strict.
  for (int i = 0; i < descriptor->nested_type_count(); ++i) {
    const Descriptor* nested_desc = descriptor->nested_type(i);
    if (groups.find(nested_desc) != groups.end()) {
      GatherOccupiedFieldRanges(nested_desc, ranges, nested_messages);
    } else {
      nested_messages->push_back(nested_desc);
    }
  }
}

// Utility function for PrintFreeFieldNumbers.
// Actually prints the formatted free field numbers for given message name and
// occupied ranges.
void FormatFreeFieldNumbers(const std::string& name,
                            const std::set<FieldRange>& ranges) {
  std::string output;
  StringAppendF(&output, "%-35s free:", name.c_str());
  int next_free_number = 1;
  for (std::set<FieldRange>::const_iterator i = ranges.begin();
       i != ranges.end(); ++i) {
    // This happens when groups re-use parent field numbers, in which
    // case we skip the FieldRange entirely.
    if (next_free_number >= i->second) continue;

    if (next_free_number < i->first) {
      if (next_free_number + 1 == i->first) {
        // Singleton
        StringAppendF(&output, " %d", next_free_number);
      } else {
        // Range
        StringAppendF(&output, " %d-%d", next_free_number, i->first - 1);
      }
    }
    next_free_number = i->second;
  }
  if (next_free_number <= FieldDescriptor::kMaxNumber) {
    StringAppendF(&output, " %d-INF", next_free_number);
  }
  std::cout << output << std::endl;
}

}  // namespace

void CommandLineInterface::PrintFreeFieldNumbers(const Descriptor* descriptor) {
  std::set<FieldRange> ranges;
  std::vector<const Descriptor*> nested_messages;
  GatherOccupiedFieldRanges(descriptor, &ranges, &nested_messages);

  for (int i = 0; i < nested_messages.size(); ++i) {
    PrintFreeFieldNumbers(nested_messages[i]);
  }
  FormatFreeFieldNumbers(descriptor->full_name(), ranges);
}


}  // namespace compiler
}  // namespace protobuf
}  // namespace google
