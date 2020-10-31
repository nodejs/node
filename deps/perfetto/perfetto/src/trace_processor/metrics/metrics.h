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

#ifndef SRC_TRACE_PROCESSOR_METRICS_METRICS_H_
#define SRC_TRACE_PROCESSOR_METRICS_METRICS_H_

#include <sqlite3.h>

#include <unordered_map>
#include <vector>

#include "perfetto/ext/base/string_view.h"
#include "perfetto/protozero/field.h"
#include "perfetto/protozero/message.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/trace_processor/util/descriptors.h"

#include "protos/perfetto/trace_processor/metrics_impl.pbzero.h"

namespace perfetto {
namespace trace_processor {
namespace metrics {

// A description of a SQL metric in C++.
struct SqlMetricFile {
  // The path of this file with the root at the metrics root.
  std::string path;

  // The field in the output proto which will be filled by the result of
  // querying the table specified by |output_table_name|.
  // Optional because not all protos need to have a field associated with them
  // in the root proto; most files will be just be run using RUN_METRIC by
  // other files.
  base::Optional<std::string> proto_field_name;

  // The table name which will be created by the SQL below to read the proto
  // bytes from.
  // Should only be set when |proto_field_name| is set.
  base::Optional<std::string> output_table_name;

  // The SQL run by this metric.
  std::string sql;
};

// Helper class to build a nested (metric) proto checking the schema against
// a descriptor.
// Visible for testing.
class ProtoBuilder {
 public:
  ProtoBuilder(const ProtoDescriptor*);

  util::Status AppendSqlValue(const std::string& field_name,
                              const SqlValue& value);

  // Note: all external callers to these functions should not
  // |is_inside_repeated| to this function and instead rely on the default
  // value.
  util::Status AppendLong(const std::string& field_name,
                          int64_t value,
                          bool is_inside_repeated = false);
  util::Status AppendDouble(const std::string& field_name,
                            double value,
                            bool is_inside_repeated = false);
  util::Status AppendString(const std::string& field_name,
                            base::StringView value,
                            bool is_inside_repeated = false);
  util::Status AppendBytes(const std::string& field_name,
                           const uint8_t* data,
                           size_t size,
                           bool is_inside_repeated = false);

  // Returns the serialized |protos::ProtoBuilderResult| with the built proto
  // as the nested |protobuf| message.
  // Note: no other functions should be called on this class after this method
  // is called.
  std::vector<uint8_t> SerializeToProtoBuilderResult();

  // Returns the serialized version of the raw message being built.
  // This function should only be used at the top level where type checking is
  // no longer important because the proto will be returned as is. In all other
  // instances, prefer |SerializeToProtoBuilderResult()| instead.
  // Note: no other functions should be called on this class after this method
  // is called.
  std::vector<uint8_t> SerializeRaw();

 private:
  util::Status AppendSingleMessage(const FieldDescriptor& field,
                                   const uint8_t* ptr,
                                   size_t size);

  util::Status AppendRepeated(const FieldDescriptor& field,
                              const uint8_t* ptr,
                              size_t size);

  const ProtoDescriptor* descriptor_ = nullptr;
  protozero::HeapBuffered<protozero::Message> message_;
};

// Helper class to combine a set of repeated fields into a single proto blob
// to return to SQLite.
// Visible for testing.
class RepeatedFieldBuilder {
 public:
  RepeatedFieldBuilder();

  util::Status AddSqlValue(SqlValue value);

  void AddLong(int64_t value);
  void AddDouble(double value);
  void AddString(base::StringView value);
  void AddBytes(const uint8_t* data, size_t size);

  // Returns the serialized |protos::ProtoBuilderResult| with the set of
  // repeated fields as |repeated_values| in the proto.
  // Note: no other functions should be called on this class after this method
  // is called.
  std::vector<uint8_t> SerializeToProtoBuilderResult();

 private:
  bool has_data_ = false;

  protozero::HeapBuffered<protos::pbzero::ProtoBuilderResult> message_;
  protos::pbzero::RepeatedBuilderResult* repeated_ = nullptr;
};

// Replaces templated variables inside |raw_text| using the substitution given
// by |substitutions| writing the result to |out|.
// The syntax followed is a cut-down variant of Jinja. This means variables that
// are to be replaced use {{variable-name}} in the raw text with subsitutions
// containing a mapping from (variable-name -> replacement).
int TemplateReplace(
    const std::string& raw_text,
    const std::unordered_map<std::string, std::string>& substitutions,
    std::string* out);

// These functions implement the RepeatedField SQL aggregate functions.
void RepeatedFieldStep(sqlite3_context* ctx, int argc, sqlite3_value** argv);
void RepeatedFieldFinal(sqlite3_context* ctx);

// Context struct for the below function.
struct BuildProtoContext {
  TraceProcessor* tp;
  const DescriptorPool* pool;
  const ProtoDescriptor* desc;
};

// This function implements all the proto creation functions.
void BuildProto(sqlite3_context* ctx, int argc, sqlite3_value** argv);

// Context struct for the below function.
struct RunMetricContext {
  TraceProcessor* tp;
  std::vector<SqlMetricFile>* metrics;
};

// This function implements the RUN_METRIC SQL function.
void RunMetric(sqlite3_context* ctx, int argc, sqlite3_value** argv);

util::Status ComputeMetrics(TraceProcessor* impl,
                            const std::vector<std::string> metrics_to_compute,
                            const std::vector<SqlMetricFile>& metrics,
                            const ProtoDescriptor& root_descriptor,
                            std::vector<uint8_t>* metrics_proto);

}  // namespace metrics
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_METRICS_METRICS_H_
