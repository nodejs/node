// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zlib/google/zip_writer.h"

#include "base/files/file.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/zlib/google/zip_internal.h"

namespace zip {
namespace internal {

bool ZipWriter::ShouldContinue() {
  if (!progress_callback_)
    return true;

  const base::TimeTicks now = base::TimeTicks::Now();
  if (next_progress_report_time_ > now)
    return true;

  next_progress_report_time_ = now + progress_period_;
  if (progress_callback_.Run(progress_))
    return true;

  LOG(ERROR) << "Cancelling ZIP creation";
  return false;
}

bool ZipWriter::AddFileContent(const base::FilePath& path, base::File file) {
  char buf[zip::internal::kZipBufSize];

  while (ShouldContinue()) {
    const int num_bytes =
        file.ReadAtCurrentPos(buf, zip::internal::kZipBufSize);

    if (num_bytes < 0) {
      DPLOG(ERROR) << "Cannot read file '" << path << "'";
      return false;
    }

    if (num_bytes == 0)
      return true;

    if (zipWriteInFileInZip(zip_file_, buf, num_bytes) != ZIP_OK) {
      DLOG(ERROR) << "Cannot write data from file '" << path << "' to ZIP";
      return false;
    }

    progress_.bytes += num_bytes;
  }

  return false;
}

bool ZipWriter::OpenNewFileEntry(const base::FilePath& path,
                                 bool is_directory,
                                 base::Time last_modified) {
  std::string str_path = path.AsUTF8Unsafe();
#if defined(OS_WIN)
  base::ReplaceSubstringsAfterOffset(&str_path, 0u, "\\", "/");
#endif
  if (is_directory)
    str_path += "/";

  return zip::internal::ZipOpenNewFileInZip(zip_file_, str_path, last_modified);
}

bool ZipWriter::CloseNewFileEntry() {
  return zipCloseFileInZip(zip_file_) == ZIP_OK;
}

bool ZipWriter::AddFileEntry(const base::FilePath& path, base::File file) {
  base::File::Info file_info;
  if (!file.GetInfo(&file_info))
    return false;

  if (!OpenNewFileEntry(path, /*is_directory=*/false, file_info.last_modified))
    return false;

  const bool success = AddFileContent(path, std::move(file));
  progress_.files++;
  return CloseNewFileEntry() && success;
}

bool ZipWriter::AddDirectoryEntry(const base::FilePath& path,
                                  base::Time last_modified) {
  progress_.directories++;
  return OpenNewFileEntry(path, /*is_directory=*/true, last_modified) &&
         CloseNewFileEntry() && ShouldContinue();
}

#if defined(OS_POSIX)
// static
std::unique_ptr<ZipWriter> ZipWriter::CreateWithFd(
    int zip_file_fd,
    const base::FilePath& root_dir,
    FileAccessor* file_accessor) {
  DCHECK(zip_file_fd != base::kInvalidPlatformFile);
  zipFile zip_file =
      internal::OpenFdForZipping(zip_file_fd, APPEND_STATUS_CREATE);

  if (!zip_file) {
    DLOG(ERROR) << "Cannot create ZIP file for FD " << zip_file_fd;
    return nullptr;
  }

  return std::unique_ptr<ZipWriter>(
      new ZipWriter(zip_file, root_dir, file_accessor));
}
#endif

// static
std::unique_ptr<ZipWriter> ZipWriter::Create(
    const base::FilePath& zip_file_path,
    const base::FilePath& root_dir,
    FileAccessor* file_accessor) {
  DCHECK(!zip_file_path.empty());
  zipFile zip_file = internal::OpenForZipping(zip_file_path.AsUTF8Unsafe(),
                                              APPEND_STATUS_CREATE);

  if (!zip_file) {
    DLOG(ERROR) << "Cannot create ZIP file '" << zip_file_path << "'";
    return nullptr;
  }

  return std::unique_ptr<ZipWriter>(
      new ZipWriter(zip_file, root_dir, file_accessor));
}

ZipWriter::ZipWriter(zipFile zip_file,
                     const base::FilePath& root_dir,
                     FileAccessor* file_accessor)
    : zip_file_(zip_file), root_dir_(root_dir), file_accessor_(file_accessor) {}

ZipWriter::~ZipWriter() {
  if (zip_file_)
    zipClose(zip_file_, nullptr);
}

bool ZipWriter::WriteEntries(Paths paths) {
  return AddEntries(paths) && Close();
}

bool ZipWriter::Close() {
  const bool success = zipClose(zip_file_, nullptr) == ZIP_OK;
  zip_file_ = nullptr;

  // Call the progress callback one last time with the final progress status.
  if (progress_callback_ && !progress_callback_.Run(progress_)) {
    LOG(ERROR) << "Cancelling ZIP creation at the end";
    return false;
  }

  return success;
}

bool ZipWriter::AddEntries(Paths paths) {
  // Constructed outside the loop in order to reuse its internal buffer.
  std::vector<base::FilePath> absolute_paths;

  while (!paths.empty()) {
    // Work with chunks of 50 paths at most.
    const size_t n = std::min<size_t>(paths.size(), 50);
    const Paths relative_paths = paths.subspan(0, n);
    paths = paths.subspan(n, paths.size() - n);

    // FileAccessor requires absolute paths.
    absolute_paths.clear();
    absolute_paths.reserve(n);
    for (const base::FilePath& relative_path : relative_paths) {
      absolute_paths.push_back(root_dir_.Append(relative_path));
    }

    DCHECK_EQ(relative_paths.size(), n);
    DCHECK_EQ(absolute_paths.size(), n);

    // We don't know which paths are files and which ones are directories, and
    // we want to avoid making a call to file_accessor_ for each entry. Try to
    // open all of the paths as files. We'll get invalid file descriptors for
    // directories.
    std::vector<base::File> files =
        file_accessor_->OpenFilesForReading(absolute_paths);
    DCHECK_EQ(files.size(), n);

    for (size_t i = 0; i < n; i++) {
      const base::FilePath& relative_path = relative_paths[i];
      const base::FilePath& absolute_path = absolute_paths[i];
      base::File& file = files[i];

      if (file.IsValid()) {
        if (!AddFileEntry(relative_path, std::move(file))) {
          LOG(ERROR) << "Cannot add file '" << relative_path << "' to ZIP";
          return false;
        }
      } else {
        // Either directory or missing file.
        const base::Time last_modified =
            file_accessor_->GetLastModifiedTime(absolute_path);
        if (last_modified.is_null()) {
          LOG(ERROR) << "Missing file or directory '" << relative_path << "'";
          return false;
        }

        DCHECK(file_accessor_->DirectoryExists(absolute_path));
        if (!AddDirectoryEntry(relative_path, last_modified)) {
          LOG(ERROR) << "Cannot add directory '" << relative_path << "' to ZIP";
          return false;
        }
      }
    }
  }

  return true;
}

}  // namespace internal
}  // namespace zip
