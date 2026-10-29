// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/output-stream.h"

namespace v8::internal {

v8::OutputStream::WriteResult FileOutputStream::WriteAsciiChunk(char* data,
                                                                int size) {
  os_.write(data, size);
  return kContinue;
}

void FileOutputStream::EndOfStream() { os_.close(); }

v8::OutputStream::WriteResult StringOutputStream::WriteAsciiChunk(char* data,
                                                                  int size) {
  os_.write(data, size);
  return kContinue;
}
std::string StringOutputStream::str() { return os_.str(); }

}  // namespace v8::internal
