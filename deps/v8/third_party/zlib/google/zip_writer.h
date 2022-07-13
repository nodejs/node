// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_ZLIB_GOOGLE_ZIP_WRITER_H_
#define THIRD_PARTY_ZLIB_GOOGLE_ZIP_WRITER_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/zlib/google/zip.h"

#if defined(USE_SYSTEM_MINIZIP)
#include <minizip/unzip.h>
#include <minizip/zip.h>
#else
#include "third_party/zlib/contrib/minizip/unzip.h"
#include "third_party/zlib/contrib/minizip/zip.h"
#endif

namespace zip {
namespace internal {

// A class used to write entries to a ZIP file and buffering the reading of
// files to limit the number of calls to the FileAccessor. This is for
// performance reasons as these calls may be expensive when IPC based).
// This class is so far internal and only used by zip.cc, but could be made
// public if needed.
//
// All methods returning a bool return true on success and false on error.
class ZipWriter {
 public:
// Creates a writer that will write a ZIP file to |zip_file_fd| or |zip_file|
// and which entries are relative to |file_accessor|'s source directory.
// All file reads are performed using |file_accessor|.
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  static std::unique_ptr<ZipWriter> CreateWithFd(int zip_file_fd,
                                                 FileAccessor* file_accessor);
#endif

  static std::unique_ptr<ZipWriter> Create(const base::FilePath& zip_file,
                                           FileAccessor* file_accessor);

  ZipWriter(const ZipWriter&) = delete;
  ZipWriter& operator=(const ZipWriter&) = delete;

  ~ZipWriter();

  // Sets the optional progress callback. The callback is called once for each
  // time |period|. The final callback is always called when the ZIP operation
  // completes.
  void SetProgressCallback(ProgressCallback callback, base::TimeDelta period) {
    progress_callback_ = std::move(callback);
    progress_period_ = std::move(period);
  }

  // Should ignore missing files and directories?
  void ContinueOnError(bool continue_on_error) {
    continue_on_error_ = continue_on_error;
  }

  // Sets the recursive flag, indicating whether the contents of subdirectories
  // should be included.
  void SetRecursive(bool b) { recursive_ = b; }

  // Sets the filter callback.
  void SetFilterCallback(FilterCallback callback) {
    filter_callback_ = std::move(callback);
  }

  // Adds the contents of a directory. If the recursive flag is set, the
  // contents of subdirectories are also added.
  bool AddDirectoryContents(const base::FilePath& path);

  // Adds the entries at |paths| to the ZIP file. These can be a mixed bag of
  // files and directories. If the recursive flag is set, the contents of
  // subdirectories is also added.
  bool AddMixedEntries(Paths paths);

  // Closes the ZIP file.
  bool Close();

 private:
  // Takes ownership of |zip_file|.
  ZipWriter(zipFile zip_file, FileAccessor* file_accessor);

  // Regularly called during processing to check whether zipping should continue
  // or should be cancelled.
  bool ShouldContinue();

  // Adds file content to currently open file entry.
  bool AddFileContent(const base::FilePath& path, base::File file);

  // Adds a file entry (including file contents).
  bool AddFileEntry(const base::FilePath& path, base::File file);

  // Adds file entries. All the paths should be existing files.
  bool AddFileEntries(Paths paths);

  // Adds a directory entry. If the recursive flag is set, the contents of this
  // directory are also added.
  bool AddDirectoryEntry(const base::FilePath& path);

  // Adds directory entries. All the paths should be existing directories. If
  // the recursive flag is set, the contents of these directories are also
  // added.
  bool AddDirectoryEntries(Paths paths);

  // Opens a file or directory entry.
  bool OpenNewFileEntry(const base::FilePath& path,
                        bool is_directory,
                        base::Time last_modified);

  // Closes the currently open entry.
  bool CloseNewFileEntry();

  // Filters entries.
  void Filter(std::vector<base::FilePath>* paths);

  // The actual zip file.
  zipFile zip_file_;

  // Abstraction over file access methods used to read files.
  FileAccessor* const file_accessor_;

  // Progress stats.
  Progress progress_;

  // Optional progress callback.
  ProgressCallback progress_callback_;

  // Optional progress reporting period.
  base::TimeDelta progress_period_;

  // Next time to report progress.
  base::TimeTicks next_progress_report_time_ = base::TimeTicks::Now();

  // Filter used to exclude files from the ZIP file.
  FilterCallback filter_callback_;

  // Should recursively add directories?
  bool recursive_ = false;

  // Should ignore missing files and directories?
  bool continue_on_error_ = false;
};

}  // namespace internal
}  // namespace zip

#endif  // THIRD_PARTY_ZLIB_GOOGLE_ZIP_WRITER_H_
