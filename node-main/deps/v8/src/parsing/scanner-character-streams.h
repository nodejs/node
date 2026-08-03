// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_SCANNER_CHARACTER_STREAMS_H_
#define V8_PARSING_SCANNER_CHARACTER_STREAMS_H_

#include <memory>

#include "include/v8-script.h"  // for v8::ScriptCompiler
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class Utf16CharacterStream;
class RuntimeCallStats;

class V8_EXPORT_PRIVATE ScannerStream {
 public:
  static Utf16CharacterStream* For(Isolate* isolate, Handle<String> data);
  static Utf16CharacterStream* For(Isolate* isolate, Handle<String> data,
                                   int start_pos, int end_pos);
  static Utf16CharacterStream* For(
      ScriptCompiler::ExternalSourceStream* source_stream,
      ScriptCompiler::StreamedSource::Encoding encoding);

  static std::unique_ptr<Utf16CharacterStream> ForTesting(const char* data);
  static std::unique_ptr<Utf16CharacterStream> ForTesting(const char* data,
                                                          size_t length);
  static std::unique_ptr<Utf16CharacterStream> ForTesting(const uint16_t* data,
                                                          size_t length);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_SCANNER_CHARACTER_STREAMS_H_
