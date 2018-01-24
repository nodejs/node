// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_BACKGROUND_PARSING_TASK_H_
#define V8_PARSING_BACKGROUND_PARSING_TASK_H_

#include <memory>

#include "include/v8.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/semaphore.h"
#include "src/compiler.h"
#include "src/parsing/parse-info.h"
#include "src/unicode-cache.h"

namespace v8 {
namespace internal {

class Parser;
class ScriptData;
class TimedHistogram;

// Internal representation of v8::ScriptCompiler::StreamedSource. Contains all
// data which needs to be transmitted between threads for background parsing,
// finalizing it on the main thread, and compiling on the main thread.
struct StreamedSource {
  StreamedSource(ScriptCompiler::ExternalSourceStream* source_stream,
                 ScriptCompiler::StreamedSource::Encoding encoding)
      : source_stream(source_stream), encoding(encoding) {}

  void Release();

  // Internal implementation of v8::ScriptCompiler::StreamedSource.
  std::unique_ptr<ScriptCompiler::ExternalSourceStream> source_stream;
  ScriptCompiler::StreamedSource::Encoding encoding;
  std::unique_ptr<ScriptCompiler::CachedData> cached_data;

  // Data needed for parsing, and data needed to to be passed between thread
  // between parsing and compilation. These need to be initialized before the
  // compilation starts.
  UnicodeCache unicode_cache;
  std::unique_ptr<ParseInfo> info;
  std::unique_ptr<Parser> parser;

  // Data needed for finalizing compilation after background compilation.
  std::unique_ptr<CompilationJob> outer_function_job;
  CompilationJobList inner_function_jobs;

  // Prevent copying.
  StreamedSource(const StreamedSource&) = delete;
  StreamedSource& operator=(const StreamedSource&) = delete;
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
  ScriptData* script_data_;
  AccountingAllocator* allocator_;
  TimedHistogram* timer_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_BACKGROUND_PARSING_TASK_H_
