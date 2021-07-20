// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

// Abstraction for file access operation required by Zip().
// Can be passed to the ZipParams for providing custom access to the files,
// for example over IPC.
// If none is provided, the files are accessed directly.
// All parameters paths are expected to be absolute.
class FileAccessor {
 public:
  virtual ~FileAccessor() = default;

  struct DirectoryContentEntry {
    base::FilePath path;
    bool is_directory = false;
  };

  // Opens files specified in |paths|.
  // Directories should be mapped to invalid files.
  virtual std::vector<base::File> OpenFilesForReading(
      const std::vector<base::FilePath>& paths) = 0;

  virtual bool DirectoryExists(const base::FilePath& path) = 0;
  virtual std::vector<DirectoryContentEntry> ListDirectoryContent(
      const base::FilePath& dir_path) = 0;
  virtual base::Time GetLastModifiedTime(const base::FilePath& path) = 0;
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

using Paths = base::span<const base::FilePath>;

// ZIP creation parameters and options.
struct ZipParams {
  // Source directory.
  base::FilePath src_dir;

  // Destination file path.
  // Either dest_file or dest_fd should be set, but not both.
  base::FilePath dest_file;

#if defined(OS_POSIX)
  // Destination file passed a file descriptor.
  // Either dest_file or dest_fd should be set, but not both.
  int dest_fd = base::kInvalidPlatformFile;
#endif

  // The relative paths to the files that should be included in the ZIP file. If
  // this is empty, all files in |src_dir| are included.
  //
  // These paths must be relative to |src_dir| and will be used as the file
  // names in the created zip file. All files must be under |src_dir| in the
  // file system hierarchy.
  Paths src_files;

  // Filter used to exclude files from the ZIP file. Only effective when
  // |src_files| is empty.
  FilterCallback filter_callback;

  // Optional progress reporting callback.
  ProgressCallback progress_callback;

  // Progress reporting period. The final callback is always called when the ZIP
  // creation operation completes.
  base::TimeDelta progress_period;

  // Whether hidden files should be included in the ZIP file. Only effective
  // when |src_files| is empty.
  bool include_hidden_files = true;

  // Abstraction around file system access used to read files. If left null, an
  // implementation that accesses files directly is used.
  FileAccessor* file_accessor = nullptr;  // Not owned
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

#if defined(OS_POSIX)
// Zips files listed in |src_relative_paths| to destination specified by file
// descriptor |dest_fd|, without taking ownership of |dest_fd|. The paths listed
// in |src_relative_paths| are relative to the |src_dir| and will be used as the
// file names in the created zip file. All source paths must be under |src_dir|
// in the file system hierarchy.
bool ZipFiles(const base::FilePath& src_dir,
              Paths src_relative_paths,
              int dest_fd);
#endif  // defined(OS_POSIX)

// Unzip the contents of zip_file into dest_dir.
// For each file in zip_file, include it only if the callback |filter_cb|
// returns true. Otherwise omit it.
// If |log_skipped_files| is true, files skipped during extraction are printed
// to debug log.
bool UnzipWithFilterCallback(const base::FilePath& zip_file,
                             const base::FilePath& dest_dir,
                             FilterCallback filter_cb,
                             bool log_skipped_files);

// Unzip the contents of zip_file, using the writers provided by writer_factory.
// For each file in zip_file, include it only if the callback |filter_cb|
// returns true. Otherwise omit it.
// If |log_skipped_files| is true, files skipped during extraction are printed
// to debug log.
typedef base::RepeatingCallback<std::unique_ptr<WriterDelegate>(
    const base::FilePath&)>
    WriterFactory;
typedef base::RepeatingCallback<bool(const base::FilePath&)> DirectoryCreator;
bool UnzipWithFilterAndWriters(const base::PlatformFile& zip_file,
                               WriterFactory writer_factory,
                               DirectoryCreator directory_creator,
                               FilterCallback filter_cb,
                               bool log_skipped_files);

// Unzip the contents of zip_file into dest_dir.
bool Unzip(const base::FilePath& zip_file, const base::FilePath& dest_dir);

}  // namespace zip

#endif  // THIRD_PARTY_ZLIB_GOOGLE_ZIP_H_
