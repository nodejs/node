#include "inspector_profiler.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "diagnosticfilename-inl.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_file.h"
#include "node_internals.h"
#include "util-inl.h"
#include "uv.h"
#include "v8-inspector.h"

#include <cinttypes>
#include <limits>
#include <sstream>
#include "simdutf.h"

namespace node {
namespace profiler {

using errors::TryCatchScope;
using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

using v8_inspector::StringView;

V8ProfilerConnection::V8ProfilerConnection(Environment* env)
    : session_(env->inspector_agent()->Connect(
          std::make_unique<V8ProfilerConnection::V8ProfilerSessionDelegate>(
              this),
          false)),
      env_(env) {}

uint64_t V8ProfilerConnection::DispatchMessage(const char* method,
                                               const char* params,
                                               bool is_profile_request) {
  std::stringstream ss;
  uint64_t id = next_id();
  // V8's inspector protocol cannot take an integer beyond the int32_t limit.
  // In practice the id we use is up to 3-5 for the profilers we have
  // here.
  CHECK_LT(id, static_cast<uint64_t>(std::numeric_limits<int32_t>::max()));
  ss << R"({ "id": )" << id;
  DCHECK(method != nullptr);
  ss << R"(, "method": ")" << method << '"';
  if (params != nullptr) {
    ss << R"(, "params": )" << params;
  }
  ss << " }";
  std::string message = ss.str();
  const uint8_t* message_data =
      reinterpret_cast<const uint8_t*>(message.c_str());
  // Save the id of the profile request to identify its response.
  if (is_profile_request) {
    profile_ids_.insert(id);
  }
  Debug(env(),
        DebugCategory::INSPECTOR_PROFILER,
        "Dispatching message %s\n",
        message);
  session_->Dispatch(StringView(message_data, message.length()));
  return id;
}

static void WriteResult(Environment* env,
                        const char* path,
                        std::string_view profile) {
  uv_buf_t buf =
      uv_buf_init(const_cast<char*>(profile.data()), profile.length());
  int ret = WriteFileSync(path, buf);
  if (ret != 0) {
    char err_buf[128];
    uv_err_name_r(ret, err_buf, sizeof(err_buf));
    fprintf(stderr, "%s: Failed to write file %s\n", err_buf, path);
    return;
  }
  Debug(env, DebugCategory::INSPECTOR_PROFILER, "Written result to %s\n", path);
}

bool StringViewToUTF8(const v8_inspector::StringView& source,
                      std::vector<char>* utf8_out,
                      size_t* utf8_length,
                      size_t padding) {
  size_t source_len = source.length();
  if (source.is8Bit()) {
    const char* latin1 = reinterpret_cast<const char*>(source.characters8());
    *utf8_length = simdutf::utf8_length_from_latin1(latin1, source_len);
    utf8_out->resize(*utf8_length + padding);
    size_t result_len =
        simdutf::convert_latin1_to_utf8(latin1, source_len, utf8_out->data());
    return *utf8_length == result_len;
  }

  const char16_t* utf16 =
      reinterpret_cast<const char16_t*>(source.characters16());
  *utf8_length = simdutf::utf8_length_from_utf16(utf16, source_len);
  utf8_out->resize(*utf8_length + padding);
  size_t result_len =
      simdutf::convert_utf16_to_utf8(utf16, source_len, utf8_out->data());
  return *utf8_length == result_len;
}

void V8ProfilerConnection::V8ProfilerSessionDelegate::SendMessageToFrontend(
    const v8_inspector::StringView& message) {
  Environment* env = connection_->env();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();
  Context::Scope context_scope(context);
  const char* type = connection_->type();

  Debug(env,
        DebugCategory::INSPECTOR_PROFILER,
        "Received %s profile message\n",
        type);

  std::vector<char> message_utf8;
  size_t message_utf8_length;
  if (!StringViewToUTF8(message,
                        &message_utf8,
                        &message_utf8_length,
                        simdjson::SIMDJSON_PADDING)) {
    fprintf(
        stderr, "Failed to convert %s profile message to UTF8 string\n", type);
    return;
  }

  simdjson::ondemand::document parsed;
  simdjson::ondemand::object response;
  if (connection_->json_parser_
          .iterate(
              message_utf8.data(), message_utf8_length, message_utf8.size())
          .get(parsed) ||
      parsed.get_object().get(response)) {
    fprintf(
        stderr, "Failed to parse %s profile result as JSON object:\n", type);
    fprintf(stderr,
            "%.*s\n",
            static_cast<int>(message_utf8_length),
            message_utf8.data());
    return;
  }

  uint64_t id;
  if (response["id"].get_uint64().get(id)) {
    fprintf(stderr, "Cannot retrieve id from %s profile response:\n", type);
    fprintf(stderr,
            "%.*s\n",
            static_cast<int>(message_utf8_length),
            message_utf8.data());
    return;
  }

  if (!connection_->HasProfileId(id)) {
    Debug(env,
          DebugCategory::INSPECTOR_PROFILER,
          "%s\n",
          std::string_view(message_utf8.data(), message_utf8_length));
    return;
  } else {
    Debug(env,
          DebugCategory::INSPECTOR_PROFILER,
          "Writing profile response (id = %" PRIu64 ")\n",
          id);
  }

  simdjson::ondemand::object result;
  // Get message.result from the response.
  if (response["result"].get_object().get(result)) {
    fprintf(stderr, "Failed to get 'result' from %s profile response:\n", type);
    fprintf(stderr,
            "%.*s\n",
            static_cast<int>(message_utf8_length),
            message_utf8.data());
    return;
  }

  connection_->WriteProfile(&result);
  connection_->RemoveProfileId(id);
}

static bool EnsureDirectory(const std::string& directory, const char* type) {
  fs::FSReqWrapSync req_wrap_sync;
  int ret = fs::MKDirpSync(nullptr, &req_wrap_sync.req, directory, 0777,
                           nullptr);
  if (ret < 0 && ret != UV_EEXIST) {
    char err_buf[128];
    uv_err_name_r(ret, err_buf, sizeof(err_buf));
    fprintf(stderr,
            "%s: Failed to create %s profile directory %s\n",
            err_buf,
            type,
            directory.c_str());
    return false;
  }
  return true;
}

std::string V8CoverageConnection::GetFilename() const {
  uint64_t timestamp =
      static_cast<uint64_t>(GetCurrentTimeInMicroseconds() / 1000);
  return SPrintF("coverage-%s-%s-%s.json",
      uv_os_getpid(),
      timestamp,
      env()->thread_id());
}

std::optional<std::string_view> V8ProfilerConnection::GetProfile(
    simdjson::ondemand::object* result) {
  simdjson::ondemand::object profile_object;
  if ((*result)["profile"].get_object().get(profile_object)) {
    fprintf(
        stderr, "'profile' from %s profile result is not an Object\n", type());
    return std::nullopt;
  }
  std::string_view profile_raw;
  if (profile_object.raw_json().get(profile_raw)) {
    fprintf(stderr,
            "Cannot get raw string of the 'profile' field from %s profile\n",
            type());
    return std::nullopt;
  }
  return profile_raw;
}

void V8ProfilerConnection::WriteProfile(simdjson::ondemand::object* result) {
  // Generate the profile output from the subclass.
  auto profile_opt = GetProfile(result);
  if (!profile_opt.has_value()) {
    return;
  }
  std::string_view profile = profile_opt.value();

  // Create the directory if necessary.
  std::string directory = GetDirectory();
  DCHECK(!directory.empty());
  if (!EnsureDirectory(directory, type())) {
    return;
  }

  std::string filename = GetFilename();
  DCHECK(!filename.empty());
  std::string path = directory + kPathSeparator + filename;

  WriteResult(env_, path.c_str(), profile);
}

// Whether a coverage entry for this script URL can appear in the test
// runner's coverage report at all. The report only considers file: URLs, and
// skips node_modules unless include globs (which only the test runner process
// knows about) could add them back.
static bool ShouldKeepScriptUrl(std::string_view url,
                                bool exclude_node_modules) {
  if (!url.starts_with("file:")) {
    return false;
  }
  if (exclude_node_modules &&
      url.find("/node_modules/") != std::string_view::npos) {
    return false;
  }
  return true;
}

// Rebuild `profile` (a serialized Profiler.takePreciseCoverage result) with
// the scripts that can never appear in the coverage report removed, so that
// they are neither written to disk nor read and parsed again by the report
// generator. The raw JSON of the kept entries is copied verbatim; only the
// "result" array is rewritten. `profile` must be backed by a buffer with at
// least simdjson::SIMDJSON_PADDING readable bytes past its end.
// Returns false if the profile could not be processed; `out` must be
// discarded in that case.
static bool FilterCoverageProfile(std::string_view profile,
                                  bool exclude_node_modules,
                                  std::string* out) {
  simdjson::ondemand::parser parser;
  simdjson::ondemand::parser script_parser;
  simdjson::ondemand::document doc;
  simdjson::ondemand::object top;
  if (parser
          .iterate(profile.data(),
                   profile.size(),
                   profile.size() + simdjson::SIMDJSON_PADDING)
          .get(doc) ||
      doc.get_object().get(top)) {
    return false;
  }

  out->reserve(profile.size() / 4);
  *out += '{';
  bool first_field = true;
  for (auto field_result : top) {
    simdjson::ondemand::field field;
    if (std::move(field_result).get(field)) {
      return false;
    }
    std::string_view key = field.escaped_key();
    if (!first_field) *out += ',';
    first_field = false;
    *out += '"';
    *out += key;
    *out += "\":";
    if (key == "result") {
      simdjson::ondemand::array scripts;
      if (field.value().get_array().get(scripts)) {
        return false;
      }
      *out += '[';
      bool first_script = true;
      for (auto script : scripts) {
        std::string_view raw;
        if (script.raw_json().get(raw)) {
          return false;
        }
        // Peek at the script's URL by re-parsing the raw slice; the slice
        // points into the padded message buffer, so over-reading
        // SIMDJSON_PADDING bytes is safe.
        simdjson::ondemand::document script_doc;
        std::string_view url;
        if (script_parser
                .iterate(raw.data(),
                         raw.size(),
                         raw.size() + simdjson::SIMDJSON_PADDING)
                .get(script_doc) ||
            script_doc["url"].get_string().get(url)) {
          return false;
        }
        if (!ShouldKeepScriptUrl(url, exclude_node_modules)) {
          continue;
        }
        if (!first_script) *out += ',';
        first_script = false;
        *out += raw;
      }
      *out += ']';
    } else {
      std::string_view raw;
      if (field.value().raw_json().get(raw)) {
        return false;
      }
      *out += raw;
    }
  }
  *out += '}';
  return true;
}

void V8CoverageConnection::WriteProfile(simdjson::ondemand::object* result) {
  Isolate* isolate = env_->isolate();
  HandleScope handle_scope(isolate);

  // This is only set up during pre-execution (when the environment variables
  // becomes available in the JS land). If it's empty, we don't have coverage
  // directory path (which is resolved in JS land at the moment) either, so
  // the best we could to is to just discard the profile and do nothing.
  // This should only happen in half-baked Environments created using the
  // embedder API.
  if (env_->source_map_cache_getter().IsEmpty()) {
    return;
  }

  Local<Context> context = env_->context();
  Context::Scope context_scope(context);

  std::string_view profile;
  if (result->raw_json().get(profile)) {
    fprintf(stderr, "Cannot get raw string of the %s profile\n", type());
    return;
  }

  // When the profile is written into a directory owned by the test runner
  // (advertised through this env var handshake so that it also reaches the
  // test child processes writing into the same directory), drop the scripts
  // its report is guaranteed to discard - node:* internals, which usually
  // dominate the profile, and node_modules unless include globs may add them
  // back - before serialization, instead of writing, re-reading and
  // re-parsing them. User-provided NODE_V8_COVERAGE directories always
  // receive the full profile.
  bool filter = false;
  bool exclude_node_modules = false;
  {
    std::optional<std::string> filter_dir =
        env_->env_vars()->Get("NODE_TEST_COVERAGE_FILTER_DIR");
    if (filter_dir.has_value() && !filter_dir->empty() &&
        *filter_dir == env_->coverage_directory()) {
      filter = true;
      exclude_node_modules =
          env_->env_vars()
              ->Get("NODE_TEST_COVERAGE_EXCLUDE_NODE_MODULES")
              .has_value();
    }
  }

  std::string filtered_profile;
  if (filter) {
    if (FilterCoverageProfile(profile, exclude_node_modules,
                              &filtered_profile)) {
      profile = filtered_profile;
    } else {
      // Fall back to the full profile; the report filters it again anyway.
      fprintf(stderr,
              "Failed to filter %s profile, writing it in full\n",
              type());
    }
  }

  // Gather source-map cache information to append to the coverage object.
  Local<Value> source_map_cache_v;
  {
    TryCatchScope try_catch(env());
    {
      Isolate::AllowJavascriptExecutionScope allow_js_here(isolate);
      Local<Function> source_map_cache_getter = env_->source_map_cache_getter();
      if (!source_map_cache_getter->Call(
              context, Undefined(isolate), 0, nullptr)
              .ToLocal(&source_map_cache_v)) {
        return;
      }
    }
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      PrintCaughtException(isolate, context, try_catch);
    }
  }

  // Create the directory if necessary.
  std::string directory = GetDirectory();
  DCHECK(!directory.empty());
  if (!EnsureDirectory(directory, type())) {
    return;
  }

  std::string filename = GetFilename();
  DCHECK(!filename.empty());
  std::string path = directory + kPathSeparator + filename;

  // Only insert source map cache when there's source map data at all.
  if (!source_map_cache_v->IsUndefined()) {
    // Apply the same filter to the source map cache: entries for scripts
    // that were dropped above can never be looked up by the report. The
    // getter builds a fresh object on every call, so it is safe to prune.
    if (filter && source_map_cache_v->IsObject()) {
      Local<Object> cache_obj = source_map_cache_v.As<Object>();
      Local<Array> names;
      if (cache_obj->GetOwnPropertyNames(context).ToLocal(&names)) {
        for (uint32_t i = 0; i < names->Length(); i++) {
          Local<Value> cache_key;
          if (!names->Get(context, i).ToLocal(&cache_key)) {
            break;
          }
          Utf8Value key_utf8(isolate, cache_key);
          if (!ShouldKeepScriptUrl(key_utf8.ToStringView(),
                                   exclude_node_modules)) {
            if (cache_obj->Delete(context, cache_key).IsNothing()) {
              break;
            }
          }
        }
      }
    }

    // Serialize only the source map cache and splice it in front of the
    // profile's closing brace, instead of round-tripping the entire profile
    // through V8's JSON parser and serializer.
    Local<String> cache_json;
    if (!v8::JSON::Stringify(context, source_map_cache_v)
             .ToLocal(&cache_json)) {
      fprintf(stderr, "Failed to stringify %s source map cache\n", type());
      return;
    }
    Utf8Value cache_utf8(isolate, cache_json);
    size_t end = profile.find_last_of('}');
    if (end == std::string_view::npos) {
      fprintf(stderr, "Malformed %s profile result\n", type());
      return;
    }
    std::string_view head = profile.substr(0, end);
    std::string output;
    output.reserve(head.length() + cache_utf8.length() + 32);
    output += head;
    // `head` is the profile object minus its closing brace; it always
    // contains at least the "result" field, so a separating comma is needed.
    output += ",\"source-map-cache\":";
    output += cache_utf8.ToStringView();
    output += '}';
    WriteResult(env_, path.c_str(), output);
  } else {
    WriteResult(env_, path.c_str(), profile);
  }
}

std::string V8CoverageConnection::GetDirectory() const {
  return env()->coverage_directory();
}

void V8CoverageConnection::Start() {
  DispatchMessage("Profiler.enable");
  DispatchMessage("Profiler.startPreciseCoverage",
                  R"({ "callCount": true, "detailed": true })");
}

void V8CoverageConnection::TakeCoverage() {
  DispatchMessage("Profiler.takePreciseCoverage", nullptr, true);
}

void V8CoverageConnection::StopCoverage() {
  DispatchMessage("Profiler.stopPreciseCoverage");
}

void V8CoverageConnection::End() {
  Debug(env_,
      DebugCategory::INSPECTOR_PROFILER,
      "V8CoverageConnection::End(), ending = %d\n", ending_);
  if (ending_) {
    return;
  }
  ending_ = true;
  TakeCoverage();
}

std::string V8CpuProfilerConnection::GetDirectory() const {
  return env()->cpu_prof_dir();
}

std::string V8CpuProfilerConnection::GetFilename() const {
  return env()->cpu_prof_name();
}

void V8CpuProfilerConnection::Start() {
  DispatchMessage("Profiler.enable");
  std::string params = R"({ "interval": )";
  params += std::to_string(env()->cpu_prof_interval());
  params += " }";
  DispatchMessage("Profiler.setSamplingInterval", params.c_str());
  DispatchMessage("Profiler.start");
}

void V8CpuProfilerConnection::End() {
  Debug(env_,
      DebugCategory::INSPECTOR_PROFILER,
      "V8CpuProfilerConnection::End(), ending = %d\n", ending_);
  if (ending_) {
    return;
  }
  ending_ = true;
  DispatchMessage("Profiler.stop", nullptr, true);
}

std::string V8HeapProfilerConnection::GetDirectory() const {
  return env()->heap_prof_dir();
}

std::string V8HeapProfilerConnection::GetFilename() const {
  return env()->heap_prof_name();
}

void V8HeapProfilerConnection::Start() {
  DispatchMessage("HeapProfiler.enable");
  std::string params = R"({ "samplingInterval": )";
  params += std::to_string(env()->heap_prof_interval());
  params += " }";
  DispatchMessage("HeapProfiler.startSampling", params.c_str());
}

void V8HeapProfilerConnection::End() {
  Debug(env_,
      DebugCategory::INSPECTOR_PROFILER,
      "V8HeapProfilerConnection::End(), ending = %d\n", ending_);
  if (ending_) {
    return;
  }
  ending_ = true;
  DispatchMessage("HeapProfiler.stopSampling", nullptr, true);
}

// For now, we only support coverage profiling, but we may add more
// in the future.
static void EndStartedProfilers(Environment* env) {
  // TODO(joyeechueng): merge these connections and use one session per env.
  Debug(env, DebugCategory::INSPECTOR_PROFILER, "EndStartedProfilers\n");
  V8ProfilerConnection* connection = env->cpu_profiler_connection();
  if (connection != nullptr) {
    connection->End();
  }

  connection = env->heap_profiler_connection();
  if (connection != nullptr) {
    connection->End();
  }

  connection = env->coverage_connection();
  if (connection != nullptr) {
    connection->End();
  }
}

static std::string ReplacePlaceholders(const std::string& pattern) {
  std::string result = pattern;

  static const std::unordered_map<std::string, std::function<std::string()>>
      kPlaceholderMap = {
          {"${pid}", []() { return std::to_string(uv_os_getpid()); }},
          // TODO(haramj): Add more placeholders as needed.
      };

  for (const auto& [placeholder, getter] : kPlaceholderMap) {
    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
      const std::string value = getter();
      result.replace(pos, placeholder.length(), value);
      pos += value.length();
    }
  }

  return result;
}

void StartProfilers(Environment* env) {
  AtExit(env, [](void* env) {
    EndStartedProfilers(static_cast<Environment*>(env));
  }, env);

  std::string coverage_str =
      env->env_vars()->Get("NODE_V8_COVERAGE").value_or(std::string());
  if (!coverage_str.empty() || env->options()->test_runner_coverage) {
    CHECK_NULL(env->coverage_connection());
    env->set_coverage_connection(std::make_unique<V8CoverageConnection>(env));
    env->coverage_connection()->Start();
  }
  if (env->options()->cpu_prof) {
    const std::string& dir = env->options()->cpu_prof_dir;
    env->set_cpu_prof_interval(env->options()->cpu_prof_interval);
    env->set_cpu_prof_dir(dir.empty() ? Environment::GetCwd(env->exec_path())
                                      : dir);
    if (env->options()->cpu_prof_name.empty()) {
      DiagnosticFilename filename(env, "CPU", "cpuprofile");
      env->set_cpu_prof_name(*filename);
    } else {
      std::string resolved_name =
          ReplacePlaceholders(env->options()->cpu_prof_name);
      env->set_cpu_prof_name(resolved_name);
    }
    CHECK_NULL(env->cpu_profiler_connection());
    env->set_cpu_profiler_connection(
        std::make_unique<V8CpuProfilerConnection>(env));
    env->cpu_profiler_connection()->Start();
  }
  if (env->options()->heap_prof) {
    const std::string& dir = env->options()->heap_prof_dir;
    env->set_heap_prof_interval(env->options()->heap_prof_interval);
    env->set_heap_prof_dir(dir.empty() ? Environment::GetCwd(env->exec_path())
                                       : dir);
    if (env->options()->heap_prof_name.empty()) {
      DiagnosticFilename filename(env, "Heap", "heapprofile");
      env->set_heap_prof_name(*filename);
    } else {
      env->set_heap_prof_name(env->options()->heap_prof_name);
    }
    env->set_heap_profiler_connection(
        std::make_unique<profiler::V8HeapProfilerConnection>(env));
    env->heap_profiler_connection()->Start();
  }
}

static void SetCoverageDirectory(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  Environment* env = Environment::GetCurrent(args);
  node::Utf8Value directory(env->isolate(), args[0].As<String>());
  env->set_coverage_directory(*directory);
}


static void SetSourceMapCacheGetter(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsFunction());
  Environment* env = Environment::GetCurrent(args);
  env->set_source_map_cache_getter(args[0].As<Function>());
}

static void StartCoverage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Debug(env,
        DebugCategory::INSPECTOR_PROFILER,
        "StartCoverage, connection %s nullptr\n",
        env->coverage_connection() == nullptr ? "==" : "!=");

  if (env->coverage_connection() != nullptr) {
    return;
  }

  // The parent of `--test --test-isolation=process` intentionally has no
  // inspector (see Environment::should_create_inspector); workers handle
  // coverage themselves. Without an inspector, V8CoverageConnection would
  // get a null session and crash on the first DispatchMessage.
  if (!env->should_create_inspector()) {
    return;
  }

  env->set_coverage_connection(std::make_unique<V8CoverageConnection>(env));
  env->coverage_connection()->Start();
}

static void TakeCoverage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  V8CoverageConnection* connection = env->coverage_connection();

  Debug(
    env,
    DebugCategory::INSPECTOR_PROFILER,
    "TakeCoverage, connection %s nullptr\n",
    connection == nullptr ? "==" : "!=");

  if (connection != nullptr) {
    Debug(env, DebugCategory::INSPECTOR_PROFILER, "taking coverage\n");
    connection->TakeCoverage();
  }
}

static void StopCoverage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  V8CoverageConnection* connection = env->coverage_connection();

  Debug(env,
        DebugCategory::INSPECTOR_PROFILER,
        "StopCoverage, connection %s nullptr\n",
        connection == nullptr ? "==" : "!=");

  if (connection != nullptr) {
    Debug(env, DebugCategory::INSPECTOR_PROFILER, "Stopping coverage\n");
    connection->StopCoverage();
  }
}

static void EndCoverage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  V8CoverageConnection* connection = env->coverage_connection();

  Debug(env,
        DebugCategory::INSPECTOR_PROFILER,
        "EndCoverage, connection %s nullptr\n",
        connection == nullptr ? "==" : "!=");

  if (connection != nullptr) {
    Debug(env, DebugCategory::INSPECTOR_PROFILER, "Ending coverage\n");
    connection->End();
  }
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  SetMethod(context, target, "setCoverageDirectory", SetCoverageDirectory);
  SetMethod(
      context, target, "setSourceMapCacheGetter", SetSourceMapCacheGetter);
  SetMethod(context, target, "startCoverage", StartCoverage);
  SetMethod(context, target, "takeCoverage", TakeCoverage);
  SetMethod(context, target, "stopCoverage", StopCoverage);
  SetMethod(context, target, "endCoverage", EndCoverage);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(SetCoverageDirectory);
  registry->Register(SetSourceMapCacheGetter);
  registry->Register(StartCoverage);
  registry->Register(TakeCoverage);
  registry->Register(StopCoverage);
  registry->Register(EndCoverage);
}

}  // namespace profiler
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(profiler, node::profiler::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(profiler,
                                node::profiler::RegisterExternalReferences)
