// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zlib/google/zip.h"

#include <queue>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "third_party/zlib/google/zip_internal.h"
#include "third_party/zlib/google/zip_reader.h"
#include "third_party/zlib/google/zip_writer.h"

namespace zip {
namespace {

bool IsHiddenFile(const base::FilePath& file_path) {
  return file_path.BaseName().value()[0] == '.';
}

bool ExcludeNoFilesFilter(const base::FilePath& file_path) {
  return true;
}

bool ExcludeHiddenFilesFilter(const base::FilePath& file_path) {
  return !IsHiddenFile(file_path);
}

// Creates a directory at |extract_dir|/|entry_path|, including any parents.
bool CreateDirectory(const base::FilePath& extract_dir,
                     const base::FilePath& entry_path) {
  return base::CreateDirectory(extract_dir.Append(entry_path));
}

// Creates a WriterDelegate that can write a file at |extract_dir|/|entry_path|.
std::unique_ptr<WriterDelegate> CreateFilePathWriterDelegate(
    const base::FilePath& extract_dir,
    const base::FilePath& entry_path) {
  return std::make_unique<FilePathWriterDelegate>(
      extract_dir.Append(entry_path));
}

class DirectFileAccessor : public FileAccessor {
 public:
  ~DirectFileAccessor() override = default;

  std::vector<base::File> OpenFilesForReading(
      const std::vector<base::FilePath>& paths) override {
    std::vector<base::File> files;

    for (const auto& path : paths) {
      base::File file;
      if (base::PathExists(path) && !base::DirectoryExists(path)) {
        file = base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
      }
      files.push_back(std::move(file));
    }

    return files;
  }

  bool DirectoryExists(const base::FilePath& file) override {
    return base::DirectoryExists(file);
  }

  std::vector<DirectoryContentEntry> ListDirectoryContent(
      const base::FilePath& dir) override {
    std::vector<DirectoryContentEntry> files;

    base::FileEnumerator file_enumerator(
        dir, false /* recursive */,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    for (base::FilePath path = file_enumerator.Next(); !path.empty();
         path = file_enumerator.Next()) {
      const bool is_directory = base::DirectoryExists(path);
      files.push_back({std::move(path), is_directory});
    }

    return files;
  }

  base::Time GetLastModifiedTime(const base::FilePath& path) override {
    base::File::Info file_info;
    if (!base::GetFileInfo(path, &file_info)) {
      LOG(ERROR) << "Cannot get modification time for '" << path << "'";
    }
    return file_info.last_modified;
  }
};

}  // namespace

std::ostream& operator<<(std::ostream& out, const Progress& progress) {
  return out << progress.bytes << " bytes, " << progress.files << " files, "
             << progress.directories << " dirs";
}

bool Zip(const ZipParams& params) {
  DirectFileAccessor default_accessor;
  FileAccessor* const file_accessor = params.file_accessor ?: &default_accessor;

  Paths files_to_add = params.src_files;

  std::vector<base::FilePath> all_files;
  if (files_to_add.empty()) {
    // Perform a Breadth First Search (BFS) of the source tree. Note that the
    // BFS order might not be optimal when storing files in a ZIP (either for
    // the storing side, or for the program that will extract this ZIP).
    for (std::queue<base::FilePath> q({params.src_dir}); !q.empty(); q.pop()) {
      for (FileAccessor::DirectoryContentEntry& entry :
           file_accessor->ListDirectoryContent(q.front())) {
        // Skip hidden and filtered files.
        if ((!params.include_hidden_files && IsHiddenFile(entry.path)) ||
            (params.filter_callback && !params.filter_callback.Run(entry.path)))
          continue;

        // Store relative path.
        all_files.emplace_back();
        const bool success =
            params.src_dir.AppendRelativePath(entry.path, &all_files.back());
        DCHECK(success);

        if (entry.is_directory)
          q.push(std::move(entry.path));
      }
    }

    files_to_add = all_files;
  }

  std::unique_ptr<internal::ZipWriter> zip_writer;

#if defined(OS_POSIX)
  if (params.dest_fd != base::kInvalidPlatformFile) {
    DCHECK(params.dest_file.empty());
    zip_writer = internal::ZipWriter::CreateWithFd(
        params.dest_fd, params.src_dir, file_accessor);
    if (!zip_writer)
      return false;
  }
#endif

  if (!zip_writer) {
    zip_writer = internal::ZipWriter::Create(params.dest_file, params.src_dir,
                                             file_accessor);
    if (!zip_writer)
      return false;
  }

  zip_writer->SetProgressCallback(params.progress_callback,
                                  params.progress_period);

  return zip_writer->WriteEntries(files_to_add);
}

bool Unzip(const base::FilePath& src_file, const base::FilePath& dest_dir) {
  return UnzipWithFilterCallback(
      src_file, dest_dir, base::BindRepeating(&ExcludeNoFilesFilter), true);
}

bool UnzipWithFilterCallback(const base::FilePath& src_file,
                             const base::FilePath& dest_dir,
                             FilterCallback filter_cb,
                             bool log_skipped_files) {
  base::File file(src_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    DLOG(WARNING) << "Failed to open " << src_file.value();
    return false;
  }
  return UnzipWithFilterAndWriters(
      file.GetPlatformFile(),
      base::BindRepeating(&CreateFilePathWriterDelegate, dest_dir),
      base::BindRepeating(&CreateDirectory, dest_dir), std::move(filter_cb),
      log_skipped_files);
}

bool UnzipWithFilterAndWriters(const base::PlatformFile& src_file,
                               WriterFactory writer_factory,
                               DirectoryCreator directory_creator,
                               FilterCallback filter_cb,
                               bool log_skipped_files) {
  ZipReader reader;
  if (!reader.OpenFromPlatformFile(src_file)) {
    DLOG(WARNING) << "Failed to open src_file " << src_file;
    return false;
  }
  while (reader.HasMore()) {
    if (!reader.OpenCurrentEntryInZip()) {
      DLOG(WARNING) << "Failed to open the current file in zip";
      return false;
    }
    const base::FilePath& entry_path = reader.current_entry_info()->file_path();
    if (reader.current_entry_info()->is_unsafe()) {
      DLOG(WARNING) << "Found an unsafe file in zip " << entry_path;
      return false;
    }
    if (filter_cb.Run(entry_path)) {
      if (reader.current_entry_info()->is_directory()) {
        if (!directory_creator.Run(entry_path))
          return false;
      } else {
        std::unique_ptr<WriterDelegate> writer = writer_factory.Run(entry_path);
        if (!reader.ExtractCurrentEntry(writer.get(),
                                        std::numeric_limits<uint64_t>::max())) {
          DLOG(WARNING) << "Failed to extract " << entry_path;
          return false;
        }
      }
    } else if (log_skipped_files) {
      DLOG(WARNING) << "Skipped file " << entry_path;
    }

    if (!reader.AdvanceToNextEntry()) {
      DLOG(WARNING) << "Failed to advance to the next file";
      return false;
    }
  }
  return true;
}

bool ZipWithFilterCallback(const base::FilePath& src_dir,
                           const base::FilePath& dest_file,
                           FilterCallback filter_cb) {
  DCHECK(base::DirectoryExists(src_dir));
  return Zip({.src_dir = src_dir,
              .dest_file = dest_file,
              .filter_callback = std::move(filter_cb)});
}

bool Zip(const base::FilePath& src_dir,
         const base::FilePath& dest_file,
         bool include_hidden_files) {
  if (include_hidden_files) {
    return ZipWithFilterCallback(src_dir, dest_file,
                                 base::BindRepeating(&ExcludeNoFilesFilter));
  } else {
    return ZipWithFilterCallback(
        src_dir, dest_file, base::BindRepeating(&ExcludeHiddenFilesFilter));
  }
}

#if defined(OS_POSIX)
bool ZipFiles(const base::FilePath& src_dir,
              Paths src_relative_paths,
              int dest_fd) {
  DCHECK(base::DirectoryExists(src_dir));
  return Zip({.src_dir = src_dir,
              .dest_fd = dest_fd,
              .src_files = src_relative_paths});
}
#endif  // defined(OS_POSIX)

}  // namespace zip
