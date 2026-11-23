#include "node_profiling.h"

#include "json_utils.h"
#include "util-inl.h"

#include <memory>
#include <string_view>

namespace node {

using v8::AllocationProfile;
using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::DictionaryTemplate;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::HeapProfiler;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::Uint32;
using v8::Value;

static void BuildHeapProfileNode(Isolate* isolate,
                                 const AllocationProfile::Node* node,
                                 JSONWriter* writer) {
  size_t selfSize = 0;
  size_t selfCount = 0;
  for (const auto& allocation : node->allocations) {
    selfSize += allocation.size * allocation.count;
    selfCount += allocation.count;
  }

  writer->json_keyvalue("selfSize", selfSize);
  writer->json_keyvalue("selfCount", selfCount);
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

bool SerializeHeapProfile(Isolate* isolate, std::string& out) {
  HandleScope scope(isolate);
  HeapProfiler* profiler = isolate->GetHeapProfiler();
  std::unique_ptr<AllocationProfile> profile(profiler->GetAllocationProfile());
  if (!profile) {
    return false;
  }
  JSONWriter writer(out, true);
  writer.json_start();

  writer.json_arraystart("samples");
  for (const auto& sample : profile->GetSamples()) {
    writer.json_start();
    writer.json_keyvalue("size", sample.size * sample.count);
    writer.json_keyvalue("objectSize", sample.size);
    writer.json_keyvalue("objectCount", sample.count);
    writer.json_keyvalue("nodeId", sample.node_id);
    writer.json_keyvalue("ordinal", static_cast<double>(sample.sample_id));
    writer.json_keyvalue("isLive", sample.is_live);
    writer.json_keyvalue("sampleInterval", sample.sample_interval);
    writer.json_end();
  }
  writer.json_arrayend();

  writer.json_objectstart("head");
  BuildHeapProfileNode(isolate, profile->GetRootNode(), &writer);
  writer.json_objectend();

  writer.json_end();
  return true;
}

MaybeLocal<Array> HeapProfileSamplesToV8Value(
    Isolate* isolate,
    Local<Context> context,
    const std::vector<AllocationProfile::Sample>& samples) {
  static constexpr std::string_view names[] = {
      "size",
      "objectSize",
      "objectCount",
      "nodeId",
      "ordinal",
      "isLive",
      "sampleInterval",
  };
  auto tmpl = DictionaryTemplate::New(isolate, names);
  LocalVector<Value> result(isolate);
  result.reserve(samples.size());
  for (const auto& sample : samples) {
    MaybeLocal<Value> values[] = {
        Number::New(isolate, sample.size * sample.count),
        Number::New(isolate, sample.size),
        Integer::NewFromUnsigned(isolate, sample.count),
        Integer::NewFromUnsigned(isolate, sample.node_id),
        Number::New(isolate, static_cast<double>(sample.sample_id)),
        Boolean::New(isolate, sample.is_live),
        Number::New(isolate, static_cast<double>(sample.sample_interval)),
    };
    Local<Object> object;
    if (!NewDictionaryInstanceNullProto(context, tmpl, values)
             .ToLocal(&object)) {
      return {};
    }
    result.emplace_back(object);
  }
  return Array::New(isolate, result.data(), result.size());
}

HeapProfileOptions ParseHeapProfileOptions(
    const FunctionCallbackInfo<Value>& args) {
  HeapProfileOptions options;
  CHECK_LE(args.Length(), 3);
  if (args.Length() > 0) {
    CHECK(args[0]->IsNumber());
    options.sample_interval =
        static_cast<uint64_t>(args[0].As<Number>()->Value());
  }
  if (args.Length() > 1) {
    CHECK(args[1]->IsInt32());
    options.stack_depth = args[1].As<Int32>()->Value();
  }
  if (args.Length() > 2) {
    CHECK(args[2]->IsUint32());
    options.flags =
        static_cast<HeapProfiler::SamplingFlags>(args[2].As<Uint32>()->Value());
  }
  return options;
}

CpuProfileOptions ParseCpuProfileOptions(
    const FunctionCallbackInfo<Value>& args) {
  CpuProfileOptions options;
  CHECK_LE(args.Length(), 2);
  if (args.Length() > 0) {
    CHECK(args[0]->IsInt32());
    options.sampling_interval_us = args[0].As<Int32>()->Value();
  }
  if (args.Length() > 1) {
    CHECK(args[1]->IsUint32());
    options.max_samples = args[1].As<Uint32>()->Value();
  }
  return options;
}

}  // namespace node
