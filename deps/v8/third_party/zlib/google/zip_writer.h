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
class ZipWriter {
 public:
// Creates a writer that will write a ZIP file to |zip_file_fd|/|zip_file|
// and which entries (specified with WriteEntries) are relative to |root_dir|.
// All file reads are performed using |file_accessor|.
#if defined(OS_POSIX)
  static std::unique_ptr<ZipWriter> CreateWithFd(int zip_file_fd,
                                                 const base::FilePath& root_dir,
                                                 FileAccessor* file_accessor);
#endif
  static std::unique_ptr<ZipWriter> Create(const base::FilePath& zip_file,
                                           const base::FilePath& root_dir,
                                           FileAccessor* file_accessor);
  ~ZipWriter();

  // Sets the optional progress callback. The callback is called once for each
  // time |period|. The final callback is always called when the ZIP operation
  // completes.
  void SetProgressCallback(ProgressCallback callback, base::TimeDelta period) {
    progress_callback_ = std::move(callback);
    progress_period_ = std::move(period);
  }

  // Writes the files at |paths| to the ZIP file and closes this ZIP file.
  // The file paths must be relative to |root_dir| specified in the
  // Create method.
  // Returns true if all entries were written successfuly.
  bool WriteEntries(Paths paths);

 private:
  // Takes ownership of |zip_file|.
  ZipWriter(zipFile zip_file,
            const base::FilePath& root_dir,
            FileAccessor* file_accessor);

  // Regularly called during processing to check whether zipping should continue
  // or should be cancelled.
  bool ShouldContinue();

  // Adds the files at |paths| to the ZIP file. These FilePaths must be relative
  // to |root_dir| specified in the Create method.
  bool AddEntries(Paths paths);

  // Adds file content to currently open file entry.
  bool AddFileContent(const base::FilePath& path, base::File file);

  // Adds a file entry (including file contents).
  bool AddFileEntry(const base::FilePath& path, base::File file);

  // Adds a directory entry.
  bool AddDirectoryEntry(const base::FilePath& path, base::Time last_modified);

  // Opens a file or directory entry.
  bool OpenNewFileEntry(const base::FilePath& path,
                        bool is_directory,
                        base::Time last_modified);

  // Closes the currently open entry.
  bool CloseNewFileEntry();

  // Closes the ZIP file.
  // Returns true if successful, false otherwise (typically if an entry failed
  // to be written).
  bool Close();

  // The actual zip file.
  zipFile zip_file_;

  // Path to the directory entry paths are relative to.
  base::FilePath root_dir_;

  // Abstraction over file access methods used to read files.
  FileAccessor* file_accessor_;

  // Progress stats.
  Progress progress_;

  // Optional progress callback.
  ProgressCallback progress_callback_;

  // Optional progress reporting period.
  base::TimeDelta progress_period_;

  // Next time to report progress.
  base::TimeTicks next_progress_report_time_ = base::TimeTicks::Now();

  DISALLOW_COPY_AND_ASSIGN(ZipWriter);
};

}  // namespace internal
}  // namespace zip

#endif  // THIRD_PARTY_ZLIB_GOOGLE_ZIP_WRITER_H_
