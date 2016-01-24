// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BACKGROUND_PARSING_TASK_H_
#define V8_BACKGROUND_PARSING_TASK_H_

#include "src/base/platform/platform.h"
#include "src/base/platform/semaphore.h"
#include "src/base/smart-pointers.h"
#include "src/compiler.h"
#include "src/parsing/parser.h"

namespace v8 {
namespace internal {

// Internal representation of v8::ScriptCompiler::StreamedSource. Contains all
// data which needs to be transmitted between threads for background parsing,
// finalizing it on the main thread, and compiling on the main thread.
struct StreamedSource {
  StreamedSource(ScriptCompiler::ExternalSourceStream* source_stream,
                 ScriptCompiler::StreamedSource::Encoding encoding)
      : source_stream(source_stream), encoding(encoding) {}

  // Internal implementation of v8::ScriptCompiler::StreamedSource.
  base::SmartPointer<ScriptCompiler::ExternalSourceStream> source_stream;
  ScriptCompiler::StreamedSource::Encoding encoding;
  base::SmartPointer<ScriptCompiler::CachedData> cached_data;

  // Data needed for parsing, and data needed to to be passed between thread
  // between parsing and compilation. These need to be initialized before the
  // compilation starts.
  UnicodeCache unicode_cache;
  base::SmartPointer<Zone> zone;
  base::SmartPointer<ParseInfo> info;
  base::SmartPointer<Parser> parser;

 private:
  // Prevent copying. Not implemented.
  StreamedSource(const StreamedSource&);
  StreamedSource& operator=(const StreamedSource&);
};


class BackgroundParsingTask : public ScriptCompiler::ScriptStreamingTask {
 public:
  BackgroundParsingTask(StreamedSource* source,
                        ScriptCompiler::CompileOptions options, int stack_size,
                        Isolate* isolate);

  virtual void Run();

 private:
  StreamedSource* source_;  // Not owned.
  int stack_size_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_BACKGROUND_PARSING_TASK_H_
