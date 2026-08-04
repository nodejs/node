#include "node_profiling.h"

#include "json_utils.h"
#include "util.h"

#include <memory>

namespace node {

using v8::AllocationProfile;
using v8::HandleScope;
using v8::HeapProfiler;
using v8::Isolate;
using v8::Value;

static void BuildHeapProfileNode(Isolate* isolate,
                                 const AllocationProfile::Node* node,
                                 JSONWriter* writer) {
  size_t selfSize = 0;
  for (const auto& allocation : node->allocations)
    selfSize += allocation.size * allocation.count;

  writer->json_keyvalue("selfSize", selfSize);
  writer->json_keyvalue("id", node->node_id);
  writer->json_objectstart("callFrame");
  writer->json_keyvalue("scriptId", node->script_id);
  writer->json_keyvalue("lineNumber", node->line_number - 1);
  writer->json_keyvalue("columnNumber", node->column_number - 1);
  Utf8Value name(isolate, node->name);
  Utf8Value script_name(isolate, node->script_name);
  writer->json_keyvalue("functionName", *name);
  writer->json_keyvalue("url", *script_name);
  writer->json_objectend();

  writer->json_arraystart("children");
  for (const auto* child : node->children) {
    writer->json_start();
    BuildHeapProfileNode(isolate, child, writer);
    writer->json_end();
  }
  writer->json_arrayend();
}

bool SerializeHeapProfile(Isolate* isolate, std::ostringstream& out_stream) {
  HandleScope scope(isolate);
  HeapProfiler* profiler = isolate->GetHeapProfiler();
  std::unique_ptr<AllocationProfile> profile(profiler->GetAllocationProfile());
  if (!profile) {
    return false;
  }
  profiler->StopSamplingHeapProfiler();
  JSONWriter writer(out_stream, true);
  writer.json_start();

  writer.json_arraystart("samples");
  for (const auto& sample : profile->GetSamples()) {
    writer.json_start();
    writer.json_keyvalue("size", sample.size * sample.count);
    writer.json_keyvalue("nodeId", sample.node_id);
    writer.json_keyvalue("ordinal", static_cast<double>(sample.sample_id));
    writer.json_end();
  }
  writer.json_arrayend();

  writer.json_objectstart("head");
  BuildHeapProfileNode(isolate, profile->GetRootNode(), &writer);
  writer.json_objectend();

  writer.json_end();
  return true;
}

HeapProfileOptions ParseHeapProfileOptions(
    const v8::FunctionCallbackInfo<Value>& args) {
  HeapProfileOptions options;
  CHECK_LE(args.Length(), 3);
  if (args.Length() > 0) {
    CHECK(args[0]->IsNumber());
    options.sample_interval =
        static_cast<uint64_t>(args[0].As<v8::Number>()->Value());
  }
  if (args.Length() > 1) {
    CHECK(args[1]->IsInt32());
    options.stack_depth = args[1].As<v8::Int32>()->Value();
  }
  if (args.Length() > 2) {
    CHECK(args[2]->IsUint32());
    options.flags = static_cast<v8::HeapProfiler::SamplingFlags>(
        args[2].As<v8::Uint32>()->Value());
  }
  return options;
}

CpuProfileOptions ParseCpuProfileOptions(
    const v8::FunctionCallbackInfo<Value>& args) {
  CpuProfileOptions options;
  CHECK_LE(args.Length(), 2);
  if (args.Length() > 0) {
    CHECK(args[0]->IsInt32());
    options.sampling_interval_us = args[0].As<v8::Int32>()->Value();
  }
  if (args.Length() > 1) {
    CHECK(args[1]->IsUint32());
    options.max_samples = args[1].As<v8::Uint32>()->Value();
  }
  return options;
}

}  // namespace node
