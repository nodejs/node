// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_OUTPUT_STREAM_H_
#define V8_UTILS_OUTPUT_STREAM_H_

#include <fstream>
#include <sstream>

#include "include/v8-profiler.h"
#include "src/base/macros.h"

namespace v8::internal {

class V8_EXPORT_PRIVATE FileOutputStream : public v8::OutputStream {
 public:
  explicit FileOutputStream(const char* filename) : os_(filename) {}
  ~FileOutputStream() override { os_.close(); }

  WriteResult WriteAsciiChunk(char* data, int size) override;
  void EndOfStream() override;

 private:
  std::ofstream os_;
};

class V8_EXPORT_PRIVATE StringOutputStream : public v8::OutputStream {
 public:
  WriteResult WriteAsciiChunk(char* data, int size) override;
  void EndOfStream() override {}

  std::string str();

 private:
  std::stringstream os_;
};

}  // namespace v8::internal

#endif  // V8_UTILS_OUTPUT_STREAM_H_
