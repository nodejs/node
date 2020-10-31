/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tools/trace_to_text/trace_to_json.h"

#include <stdio.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/temp_file.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "tools/trace_to_text/utils.h"

namespace perfetto {
namespace trace_to_text {

namespace {

const char kTraceHeader[] = R"({
  "traceEvents": [],
)";

const char kTraceFooter[] = R"(,
  "controllerTraceDataKey": "systraceController"
})";

bool ExportUserspaceEvents(trace_processor::TraceProcessor* tp,
                           TraceWriter* writer) {
  fprintf(stderr, "Converting userspace events%c", kProgressChar);
  fflush(stderr);

  // Write userspace trace to a temporary file.
  // TODO(eseckler): Support streaming the result out of TP directly instead.
  auto file = base::TempFile::Create();
  char query[100];
  sprintf(query, "select export_json(\"%s\")", file.path().c_str());
  auto it = tp->ExecuteQuery(query);

  if (!it.Next()) {
    auto status = it.Status();
    PERFETTO_CHECK(!status.ok());
    PERFETTO_ELOG("Could not convert userspace events: %s", status.c_message());
    return false;
  }

  base::ScopedFstream source(fopen(file.path().c_str(), "r"));
  if (!source) {
    PERFETTO_ELOG("Could not convert userspace events: Couldn't read file %s",
                  file.path().c_str());
    return false;
  }

  char buf[BUFSIZ];
  size_t size;
  while ((size = fread(buf, sizeof(char), BUFSIZ, *source)) > 0) {
    // Skip writing the closing brace since we'll append system trace data.
    if (feof(*source))
      size--;
    writer->Write(buf, size);
  }
  return true;
}

}  // namespace

int TraceToJson(std::istream* input,
                std::ostream* output,
                bool compress,
                Keep truncate_keep,
                bool full_sort) {
  std::unique_ptr<TraceWriter> trace_writer(
      compress ? new DeflateTraceWriter(output) : new TraceWriter(output));

  trace_processor::Config config;
  config.force_full_sort = full_sort;
  std::unique_ptr<trace_processor::TraceProcessor> tp =
      trace_processor::TraceProcessor::CreateInstance(config);

  if (!ReadTrace(tp.get(), input))
    return 1;
  tp->NotifyEndOfFile();

  // TODO(eseckler): Support truncation of userspace event data.
  if (ExportUserspaceEvents(tp.get(), trace_writer.get())) {
    trace_writer->Write(",\n");
  } else {
    trace_writer->Write(kTraceHeader);
  }

  int ret = ExtractSystrace(tp.get(), trace_writer.get(),
                            /*wrapped_in_json=*/true, truncate_keep);
  if (ret)
    return ret;

  trace_writer->Write(kTraceFooter);
  return 0;
}

}  // namespace trace_to_text
}  // namespace perfetto
