// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_ZLIB_GOOGLE_REDACT_H_
#define THIRD_PARTY_ZLIB_GOOGLE_REDACT_H_

#include <ostream>

#include "base/files/file_path.h"
#include "base/logging.h"

namespace zip {

// Redacts file paths in log messages.
// Example:
// LOG(ERROR) << "Cannot open " << Redact(path);
class Redact {
 public:
  explicit Redact(const base::FilePath& path) : path_(path) {}

  friend std::ostream& operator<<(std::ostream& out, const Redact&& r) {
    return LOG_IS_ON(INFO) ? out << "'" << r.path_ << "'" : out << "(redacted)";
  }

 private:
  const base::FilePath& path_;
};

}  // namespace zip

#endif  // THIRD_PARTY_ZLIB_GOOGLE_REDACT_H_
