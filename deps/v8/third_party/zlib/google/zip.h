// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_ZLIB_GOOGLE_ZIP_H_
#define THIRD_PARTY_ZLIB_GOOGLE_ZIP_H_

#include <cstdint>
#include <ostream>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {
class File;
}

namespace zip {

class WriterDelegate;

// Paths passed as span to avoid copying them.
using Paths = base::span<const base::FilePath>;

// Abstraction for file access operation required by Zip().
//
// Can be passed to the ZipParams for providing custom access to the files,
// for example over IPC.
//
// All parameters paths are expected to be relative to the source directory.
class FileAccessor {
 public:
  virtual ~FileAccessor() = default;

  struct Info {
    bool is_directory = false;
    base::Time last_modified;
  };

  // Opens files specified in |paths|.
  // Directories should be mapped to invalid files.
  virtual bool Open(Paths paths, std::vector<base::File>* files) = 0;

  // Lists contents of a directory at |path|.
  virtual bool List(const base::FilePath& path,
                    std::vector<base::FilePath>* files,
                    std::vector<base::FilePath>* subdirs) = 0;

  // Gets info about a file or directory.
  virtual bool GetInfo(const base::FilePath& path, Info* info) = 0;
};

// Progress of a ZIP creation operation.
struct Progress {
  // Total number of bytes read from files getting zipped so far.
  std::int64_t bytes = 0;

  // Number of file entries added to the ZIP so far.
  // A file entry is added after its bytes have been processed.
  int files = 0;

  // Number of directory entries added to the ZIP so far.
  // A directory entry is added before items in it.
  int directories = 0;

  // Number of errors encountered so far (files that cannot be opened,
  // directories that cannot be listed).
  int errors = 0;
};

// Prints Progress to output stream.
std::ostream& operator<<(std::ostream& out, const Progress& progress);

// Callback reporting the progress of a ZIP creation operation.
//
// This callback returns a boolean indicating whether the ZIP creation operation
// should continue. If it returns false once, then the ZIP creation operation is
// immediately cancelled and the callback won't be called again.
using ProgressCallback = base::RepeatingCallback<bool(const Progress&)>;

using FilterCallback = base::RepeatingCallback<bool(const base::FilePath&)>;

// ZIP creation parameters and options.
struct ZipParams {
  // Source directory. Ignored if |file_accessor| is set.
  base::FilePath src_dir;

  // Abstraction around file system access used to read files.
  // If left null, an implementation that accesses files directly is used.
  FileAccessor* file_accessor = nullptr;  // Not owned

  // Destination file path.
  // Either dest_file or dest_fd should be set, but not both.
  base::FilePath dest_file;

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  // Destination file passed a file descriptor.
  // Either dest_file or dest_fd should be set, but not both.
  int dest_fd = base::kInvalidPlatformFile;
#endif

  // The relative paths to the files and directories that should be included in
  // the ZIP file. If this is empty, the whole contents of |src_dir| are
  // included.
  //
  // These paths must be relative to |src_dir| and will be used as the file
  // names in the created ZIP file. All files must be under |src_dir| in the
  // file system hierarchy.
  //
  // All the paths in |src_files| are included in the created ZIP file,
  // irrespective of |include_hidden_files| and |filter_callback|.
  Paths src_files;

  // Filter used to exclude files from the ZIP file. This is only taken in
  // account when recursively adding subdirectory contents.
  FilterCallback filter_callback;

  // Optional progress reporting callback.
  ProgressCallback progress_callback;

  // Progress reporting period. The final callback is always called when the ZIP
  // creation operation completes.
  base::TimeDelta progress_period;

  // Should add hidden files? This is only taken in account when recursively
  // adding subdirectory contents.
  bool include_hidden_files = true;

  // Should recursively add subdirectory contents?
  bool recursive = false;

  // Should ignore errors when discovering files and zipping them?
  bool continue_on_error = false;
};

// Zip files specified into a ZIP archives. The source files and ZIP destination
// files (as well as other settings) are specified in |params|.
bool Zip(const ZipParams& params);

// Zip the contents of src_dir into dest_file. src_path must be a directory.
// An entry will *not* be created in the zip for the root folder -- children
// of src_dir will be at the root level of the created zip. For each file in
// src_dir, include it only if the callback |filter_cb| returns true. Otherwise
// omit it.
bool ZipWithFilterCallback(const base::FilePath& src_dir,
                           const base::FilePath& dest_file,
                           FilterCallback filter_cb);

// Convenience method for callers who don't need to set up the filter callback.
// If |include_hidden_files| is true, files starting with "." are included.
// Otherwise they are omitted.
bool Zip(const base::FilePath& src_dir,
         const base::FilePath& dest_file,
         bool include_hidden_files);

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
// Zips files listed in |src_relative_paths| to destination specified by file
// descriptor |dest_fd|, without taking ownership of |dest_fd|. The paths listed
// in |src_relative_paths| are relative to the |src_dir| and will be used as the
// file names in the created zip file. All source paths must be under |src_dir|
// in the file system hierarchy.
bool ZipFiles(const base::FilePath& src_dir,
              Paths src_relative_paths,
              int dest_fd);
#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)

// Callback reporting the number of bytes written during Unzip.
using UnzipProgressCallback = base::RepeatingCallback<void(uint64_t bytes)>;

// Options of the Unzip function, with valid default values.
struct UnzipOptions {
  // Encoding of entry paths in the ZIP archive. By default, paths are assumed
  // to be in UTF-8.
  std::string encoding;

  // Only extract the entries for which |filter_cb| returns true. By default,
  // everything gets extracted.
  FilterCallback filter;

  // Callback to report bytes extracted from the ZIP.
  UnzipProgressCallback progress;

  // Password to decrypt the encrypted files.
  std::string password;

  // Should ignore errors when extracting files?
  bool continue_on_error = false;
};

typedef base::RepeatingCallback<std::unique_ptr<WriterDelegate>(
    const base::FilePath&)>
    WriterFactory;

typedef base::RepeatingCallback<bool(const base::FilePath&)> DirectoryCreator;

// Unzips the contents of |zip_file|, using the writers provided by
// |writer_factory|.
bool Unzip(const base::PlatformFile& zip_file,
           WriterFactory writer_factory,
           DirectoryCreator directory_creator,
           UnzipOptions options = {});

// Unzips the contents of |zip_file| into |dest_dir|.
// This function does not overwrite any existing file.
// A filename collision will result in an error.
// Therefore, |dest_dir| should initially be an empty directory.
bool Unzip(const base::FilePath& zip_file,
           const base::FilePath& dest_dir,
           UnzipOptions options = {});

}  // namespace zip

#endif  // THIRD_PARTY_ZLIB_GOOGLE_ZIP_H_
