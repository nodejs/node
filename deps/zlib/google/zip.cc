// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zlib/google/zip.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "third_party/zlib/google/redact.h"
#include "third_party/zlib/google/zip_internal.h"
#include "third_party/zlib/google/zip_reader.h"
#include "third_party/zlib/google/zip_writer.h"

namespace zip {
namespace {

bool IsHiddenFile(const base::FilePath& file_path) {
  return file_path.BaseName().value()[0] == '.';
}

// Creates a directory at |extract_dir|/|entry_path|, including any parents.
bool CreateDirectory(const base::FilePath& extract_dir,
                     const base::FilePath& entry_path) {
  const base::FilePath dir = extract_dir.Append(entry_path);
  const bool ok = base::CreateDirectory(dir);
  PLOG_IF(ERROR, !ok) << "Cannot create directory " << Redact(dir);
  return ok;
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
  explicit DirectFileAccessor(base::FilePath src_dir)
      : src_dir_(std::move(src_dir)) {}

  ~DirectFileAccessor() override = default;

  bool Open(const Paths paths, std::vector<base::File>* const files) override {
    DCHECK(files);
    files->reserve(files->size() + paths.size());

    for (const base::FilePath& path : paths) {
      DCHECK(!path.IsAbsolute());
      const base::FilePath absolute_path = src_dir_.Append(path);
      if (base::DirectoryExists(absolute_path)) {
        files->emplace_back();
        LOG(ERROR) << "Cannot open " << Redact(path) << ": It is a directory";
      } else {
        const base::File& file = files->emplace_back(
            absolute_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
        LOG_IF(ERROR, !file.IsValid())
            << "Cannot open " << Redact(path) << ": "
            << base::File::ErrorToString(file.error_details());
      }
    }

    return true;
  }

  bool List(const base::FilePath& path,
            std::vector<base::FilePath>* const files,
            std::vector<base::FilePath>* const subdirs) override {
    DCHECK(!path.IsAbsolute());
    DCHECK(files);
    DCHECK(subdirs);

    base::FileEnumerator file_enumerator(
        src_dir_.Append(path), false /* recursive */,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);

    while (!file_enumerator.Next().empty()) {
      const base::FileEnumerator::FileInfo info = file_enumerator.GetInfo();
      (info.IsDirectory() ? subdirs : files)
          ->push_back(path.Append(info.GetName()));
    }

    return true;
  }

  bool GetInfo(const base::FilePath& path, Info* const info) override {
    DCHECK(!path.IsAbsolute());
    DCHECK(info);

    base::File::Info file_info;
    if (!base::GetFileInfo(src_dir_.Append(path), &file_info)) {
      PLOG(ERROR) << "Cannot get info of " << Redact(path);
      return false;
    }

    info->is_directory = file_info.is_directory;
    info->last_modified = file_info.last_modified;

    return true;
  }

 private:
  const base::FilePath src_dir_;
};

}  // namespace

std::ostream& operator<<(std::ostream& out, const Progress& progress) {
  return out << progress.bytes << " bytes, " << progress.files << " files, "
             << progress.directories << " dirs, " << progress.errors
             << " errors";
}

bool Zip(const ZipParams& params) {
  DirectFileAccessor default_accessor(params.src_dir);
  FileAccessor* const file_accessor = params.file_accessor ?: &default_accessor;

  std::unique_ptr<internal::ZipWriter> zip_writer;

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  if (params.dest_fd != base::kInvalidPlatformFile) {
    DCHECK(params.dest_file.empty());
    zip_writer =
        internal::ZipWriter::CreateWithFd(params.dest_fd, file_accessor);
    if (!zip_writer)
      return false;
  }
#endif

  if (!zip_writer) {
    zip_writer = internal::ZipWriter::Create(params.dest_file, file_accessor);
    if (!zip_writer)
      return false;
  }

  zip_writer->SetProgressCallback(params.progress_callback,
                                  params.progress_period);
  zip_writer->SetRecursive(params.recursive);
  zip_writer->ContinueOnError(params.continue_on_error);

  if (!params.include_hidden_files || params.filter_callback)
    zip_writer->SetFilterCallback(base::BindRepeating(
        [](const ZipParams* const params, const base::FilePath& path) -> bool {
          return (params->include_hidden_files || !IsHiddenFile(path)) &&
                 (!params->filter_callback ||
                  params->filter_callback.Run(params->src_dir.Append(path)));
        },
        &params));

  if (params.src_files.empty()) {
    // No source items are specified. Zip the entire source directory.
    zip_writer->SetRecursive(true);
    if (!zip_writer->AddDirectoryContents(base::FilePath()))
      return false;
  } else {
    // Only zip the specified source items.
    if (!zip_writer->AddMixedEntries(params.src_files))
      return false;
  }

  return zip_writer->Close();
}

bool Unzip(const base::FilePath& src_file,
           const base::FilePath& dest_dir,
           UnzipOptions options) {
  base::File file(src_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    PLOG(ERROR) << "Cannot open " << Redact(src_file) << ": "
                << base::File::ErrorToString(file.error_details());
    return false;
  }

  DLOG_IF(WARNING, !base::IsDirectoryEmpty(dest_dir))
      << "ZIP extraction directory is not empty: " << dest_dir;

  return Unzip(file.GetPlatformFile(),
               base::BindRepeating(&CreateFilePathWriterDelegate, dest_dir),
               base::BindRepeating(&CreateDirectory, dest_dir),
               std::move(options));
}

bool Unzip(const base::PlatformFile& src_file,
           WriterFactory writer_factory,
           DirectoryCreator directory_creator,
           UnzipOptions options) {
  ZipReader reader;
  reader.SetEncoding(std::move(options.encoding));
  reader.SetPassword(std::move(options.password));

  if (!reader.OpenFromPlatformFile(src_file)) {
    LOG(ERROR) << "Cannot open ZIP from file handle " << src_file;
    return false;
  }

  while (const ZipReader::Entry* const entry = reader.Next()) {
    if (entry->is_unsafe) {
      LOG(ERROR) << "Found unsafe entry " << Redact(entry->path) << " in ZIP";
      if (!options.continue_on_error)
        return false;
      continue;
    }

    if (options.filter && !options.filter.Run(entry->path)) {
      VLOG(1) << "Skipped ZIP entry " << Redact(entry->path);
      continue;
    }

    if (entry->is_directory) {
      // It's a directory.
      if (!directory_creator.Run(entry->path)) {
        LOG(ERROR) << "Cannot create directory " << Redact(entry->path);
        if (!options.continue_on_error)
          return false;
      }

      continue;
    }

    // It's a file.
    std::unique_ptr<WriterDelegate> writer = writer_factory.Run(entry->path);
    if (!writer ||
        (options.progress ? !reader.ExtractCurrentEntryWithListener(
                                writer.get(), options.progress)
                          : !reader.ExtractCurrentEntry(writer.get()))) {
      LOG(ERROR) << "Cannot extract file " << Redact(entry->path)
                 << " from ZIP";
      if (!options.continue_on_error)
        return false;
    }
  }

  return reader.ok();
}

bool ZipWithFilterCallback(const base::FilePath& src_dir,
                           const base::FilePath& dest_file,
                           FilterCallback filter) {
  DCHECK(base::DirectoryExists(src_dir));
  return Zip({.src_dir = src_dir,
              .dest_file = dest_file,
              .filter_callback = std::move(filter)});
}

bool Zip(const base::FilePath& src_dir,
         const base::FilePath& dest_file,
         bool include_hidden_files) {
  return Zip({.src_dir = src_dir,
              .dest_file = dest_file,
              .include_hidden_files = include_hidden_files});
}

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
bool ZipFiles(const base::FilePath& src_dir,
              Paths src_relative_paths,
              int dest_fd) {
  DCHECK(base::DirectoryExists(src_dir));
  return Zip({.src_dir = src_dir,
              .dest_fd = dest_fd,
              .src_files = src_relative_paths});
}
#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)

}  // namespace zip
