
#include "node_snapshotable.h"
#include <iostream>
#include <sstream>
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_file.h"
#include "node_internals.h"
#include "node_main_instance.h"
#include "node_v8.h"
#include "node_v8_platform-inl.h"

#if HAVE_INSPECTOR
#include "inspector/worker_inspector.h"  // ParentInspectorHandle
#endif

namespace node {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::SnapshotCreator;
using v8::StartupData;
using v8::String;
using v8::TryCatch;
using v8::Value;

const uint64_t SnapshotData::kMagic;

class FileIO {
 public:
  explicit FileIO(FILE* file)
      : f(file),
        is_debug(per_process::enabled_debug_list.enabled(
            DebugCategory::MKSNAPSHOT)) {}

  template <typename... Args>
  void Debug(const char* format, Args&&... args) const {
    per_process::Debug(
        DebugCategory::MKSNAPSHOT, format, std::forward<Args>(args)...);
  }

  template <typename T>
  std::string ToStr(const T& arg) const {
    std::stringstream ss;
    ss << arg;
    return ss.str();
  }
  template <typename T>
  const char* GetName() const {
    return "";
  }
  FILE* f = nullptr;
  bool is_debug = false;
};

template <>
const char* FileIO::GetName<int>() const {
  return "int";
}
template <>
const char* FileIO::GetName<size_t>() const {
  return "size_t";
}
template <>
const char* FileIO::GetName<char>() const {
  return "char";
}
class FileReader : public FileIO {
 public:
  explicit FileReader(FILE* file) : FileIO(file) {}
  ~FileReader() {}

  template <typename T>
  void Read(T* out, size_t count) {
    if (is_debug) {
      Debug("Read<%s>()(%d-byte), count=%d: ", GetName<T>(), sizeof(T), count);
    }

    size_t r = fread(out, sizeof(T), count, f);

    if (is_debug) {
      std::string str = ToStr(*out);
      Debug("%s, read %d bytes\n", str.c_str(), r);
    }
  }

  template <typename T>
  T Read() {
    T result;
    Read(&result, 1);
    return result;
  }

  template <typename T>
  std::vector<T> ReadVector(size_t count) {
    if (is_debug) {
      Debug("ReadVector<%s>()(%d-byte), count=%d: ",
            GetName<T>(),
            sizeof(T),
            count);
    }

    std::vector<T> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      if (is_debug) {
        Debug("[%d] ", i);
      }
      result.push_back(Read<T>());
    }

    return result;
  }

  template <typename T>
  std::vector<T> ReadVector() {
    size_t count = Read<size_t>();
    return ReadVector<T>(count);
  }

  std::string ReadString(size_t length) {
    if (is_debug) {
      Debug("ReadString(), length=%d: ", length);
    }

    MallocedBuffer<char> buf(length + 1);
    size_t r = fread(buf.data, 1, length + 1, f);
    std::string result(buf.data, length);

    if (is_debug) {
      Debug("%s, read %d bytes\n", result.c_str(), r);
    }

    return result;
  }

  std::string ReadString() {
    size_t length = Read<size_t>();
    return ReadString(length);
  }
};

class FileWriter : public FileIO {
 public:
  explicit FileWriter(FILE* file) : FileIO(file) {}
  ~FileWriter() {}

  template <typename T>
  size_t Write(const T& data) {
    return Write(&data, 1);
  }

  template <typename T>
  size_t Write(const T* data, size_t count) {
    if (is_debug) {
      std::string str = ToStr(*data);
      Debug("Write<%s>() (%d-byte), count=%d: %s",
            GetName<T>(),
            sizeof(T),
            count,
            str.c_str());
    }

    size_t r = fwrite(data, sizeof(T), count, f);

    if (is_debug) {
      Debug(", wrote %d bytes\n", r);
    }
    return r;
  }

  template <typename T>
  size_t WriteVector(const std::vector<T>& data) {
    if (is_debug) {
      std::string str = ToStr(data);
      Debug("WriteVector<%s>() (%d-byte), count=%d: %s\n",
            GetName<T>(),
            sizeof(T),
            data.size(),
            str.c_str());
    }

    size_t count = data.size();
    size_t written_total = Write(count);
    for (size_t i = 0; i < data.size(); ++i) {
      if (is_debug) {
        Debug("[%d] ", i);
      }
      written_total += Write<T>(data[i]);
    }

    if (is_debug) {
      Debug("WriteVector<%s>() wrote %d bytes\n", GetName<T>(), written_total);
    }

    return written_total;
  }

  size_t WriteString(const std::string& data) {
    if (is_debug) {
      std::string str = ToStr(data);
      Debug("WriteString(), length=%d: %s\n", data.size(), data.c_str());
    }

    Write<size_t>(data.size());
    size_t r = fwrite(data.c_str(), 1, data.size() + 1, f);

    if (is_debug) {
      Debug("WriteString() wrote %d bytes\n", r);
    }

    return r;
  }
};

template <>
std::string FileReader::Read() {
  return ReadString();
}
template <>
size_t FileWriter::Write(const std::string& data) {
  return WriteString(data);
}

template <>
v8::StartupData FileReader::Read() {
  Debug("Read<v8::StartupData>() ");

  int length = Read<int>();
  MallocedBuffer<char> buf(length);
  Read<char>(buf.data, length);
  DCHECK_EQ(buf.size, static_cast<size_t>(length));
  char* ptr = buf.release();

  Debug("size=%d\n", buf.size);

  return v8::StartupData{ptr, length};
}

template <>
size_t FileWriter::Write(const v8::StartupData& data) {
  Debug("\nWrite<v8::StartupData>() size=%d\n", data.raw_size);

  int count = data.raw_size;
  size_t written_total = Write<int>(count);
  written_total += Write<char>(data.data, static_cast<size_t>(count));

  Debug("Write<v8::StartupData>() wrote %d bytes\n\n", written_total);
  return written_total;
}

template <>
PropInfo FileReader::Read() {
  Debug("Read<PropInfo>() ");

  PropInfo result;
  result.name = ReadString();
  result.id = Read<size_t>();
  result.index = Read<size_t>();

  if (is_debug) {
    std::string str = ToStr(result);
    Debug(" %s", str.c_str());
  }

  return result;
}

template <>
size_t FileWriter::Write(const PropInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<PropInfo>() %s ", str.c_str());
  }

  size_t written_total = WriteString(data.name);
  written_total += Write<size_t>(data.id);
  written_total += Write<size_t>(data.index);

  Debug("wrote %d bytes\n", written_total);
  return written_total;
}

template <>
AsyncHooks::SerializeInfo FileReader::Read() {
  Debug("Read<AsyncHooks::SerializeInfo>() ");

  AsyncHooks::SerializeInfo result;
  result.async_ids_stack = Read<size_t>();
  result.fields = Read<size_t>();
  result.async_id_fields = Read<size_t>();
  result.js_execution_async_resources = Read<size_t>();
  result.native_execution_async_resources = ReadVector<size_t>();

  if (is_debug) {
    std::string str = ToStr(result);
    Debug(" %s", str.c_str());
  }

  return result;
}
template <>
size_t FileWriter::Write(const AsyncHooks::SerializeInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<AsyncHooks::SerializeInfo>() %s ", str.c_str());
  }

  size_t written_total = Write<size_t>(data.async_ids_stack);
  written_total += Write<size_t>(data.fields);
  written_total += Write<size_t>(data.async_id_fields);
  written_total += Write<size_t>(data.js_execution_async_resources);
  written_total += WriteVector<size_t>(data.native_execution_async_resources);

  Debug("wrote %d bytes\n", written_total);
  return written_total;
}

template <>
TickInfo::SerializeInfo FileReader::Read() {
  Debug("Read<TickInfo::SerializeInfo>() ");

  TickInfo::SerializeInfo result;
  result.fields = Read<size_t>();

  if (is_debug) {
    std::string str = ToStr(result);
    Debug(" %s", str.c_str());
  }

  return result;
}

template <>
size_t FileWriter::Write(const TickInfo::SerializeInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<TickInfo::SerializeInfo>() %s ", str.c_str());
  }

  size_t written_total = Write<size_t>(data.fields);

  Debug("wrote %d bytes\n", written_total);
  return written_total;
}

template <>
ImmediateInfo::SerializeInfo FileReader::Read() {
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Read ImmediateInfo::SerializeInfo\n");
  ImmediateInfo::SerializeInfo result;
  result.fields = Read<size_t>();
  return result;
}

template <>
size_t FileWriter::Write(const ImmediateInfo::SerializeInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<ImmeidateInfo::SerializeInfo>() %s ", str.c_str());
  }

  size_t written_total = Write<size_t>(data.fields);

  Debug("wrote %d bytes\n", written_total);
  return written_total;
}

template <>
performance::PerformanceState::SerializeInfo FileReader::Read() {
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Read PerformanceState::SerializeInfo\n");
  performance::PerformanceState::SerializeInfo result;
  result.root = Read<size_t>();
  result.milestones = Read<size_t>();
  result.observers = Read<size_t>();
  return result;
}

template <>
size_t FileWriter::Write(
    const performance::PerformanceState::SerializeInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<PerformanceState::SerializeInfo>() %s", str.c_str());
  }

  size_t written_total = Write<size_t>(data.root);
  written_total += Write<size_t>(data.milestones);
  written_total += Write<size_t>(data.observers);

  Debug("wrote %d bytes\n", written_total);
  return written_total;
}

template <>
EnvSerializeInfo FileReader::Read() {
  per_process::Debug(DebugCategory::MKSNAPSHOT, "Read EnvSerializeInfo\n");
  EnvSerializeInfo result;
  result.bindings = ReadVector<PropInfo>();
  result.native_modules = ReadVector<std::string>();

  result.async_hooks = Read<AsyncHooks::SerializeInfo>();
  result.tick_info = Read<TickInfo::SerializeInfo>();
  result.immediate_info = Read<ImmediateInfo::SerializeInfo>();
  result.performance_state =
      Read<performance::PerformanceState::SerializeInfo>();

  result.stream_base_state = Read<size_t>();
  result.should_abort_on_uncaught_toggle = Read<size_t>();

  result.persistent_templates = ReadVector<PropInfo>();
  result.persistent_values = ReadVector<PropInfo>();

  result.context = Read<size_t>();
  return result;
}

template <>
size_t FileWriter::Write(const EnvSerializeInfo& data) {
  Debug("\nWrite<EnvSerializeInfo>()\n");

  size_t written_total = WriteVector<PropInfo>(data.bindings);
  written_total += WriteVector<std::string>(data.native_modules);

  written_total += Write<AsyncHooks::SerializeInfo>(data.async_hooks);
  written_total += Write<TickInfo::SerializeInfo>(data.tick_info);
  written_total += Write<ImmediateInfo::SerializeInfo>(data.immediate_info);
  written_total += Write<performance::PerformanceState::SerializeInfo>(
      data.performance_state);

  written_total += Write<size_t>(data.stream_base_state);
  written_total += Write<size_t>(data.should_abort_on_uncaught_toggle);

  written_total += WriteVector<PropInfo>(data.persistent_templates);
  written_total += WriteVector<PropInfo>(data.persistent_values);

  written_total += Write<size_t>(data.context);

  Debug("wrote %d bytes\n", written_total);
  return written_total;
}

void SnapshotData::ToBlob(FILE* out) {
  FileWriter w(out);
  w.Debug("SnapshotData::ToBlob()\n");
  w.Debug("Write magic %" PRIx64 "\n", kMagic);
  w.Write<uint64_t>(kMagic);
  w.Write<v8::StartupData>(blob);
  w.Debug("Write isolate_data_indices");
  w.WriteVector<size_t>(isolate_data_indices);
  w.Write<EnvSerializeInfo>(env_info);
}

void SnapshotData::FromBlob(SnapshotData* out, FILE* in) {
  FileReader r(in);
  r.Debug("SnapshotData::FromBlob()\n");
  uint64_t magic = r.Read<uint64_t>();
  r.Debug("Read magic %" PRIx64 "\n", magic);
  CHECK_EQ(magic, kMagic);
  out->blob = r.Read<v8::StartupData>();
  out->isolate_data_indices = r.ReadVector<size_t>();
  out->env_info = r.Read<EnvSerializeInfo>();
}

template <typename T>
void WriteVector(std::ostringstream* ss, const T* vec, size_t size) {
  for (size_t i = 0; i < size; i++) {
    *ss << std::to_string(vec[i]) << (i == size - 1 ? '\n' : ',');
  }
}

std::string FormatBlob(SnapshotData* data) {
  std::ostringstream ss;

  ss << R"(#include <cstddef>
#include "env.h"
#include "node_main_instance.h"
#include "v8.h"

// This file is generated by tools/snapshot. Do not edit.

namespace node {

static const char blob_data[] = {
)";
  WriteVector(&ss, data->blob.data, data->blob.raw_size);
  ss << R"(};

static const int blob_size = )"
     << data->blob.raw_size << R"(;
static v8::StartupData blob = { blob_data, blob_size };
)";

  ss << R"(v8::StartupData* NodeMainInstance::GetEmbeddedSnapshotBlob() {
  return &blob;
}

static const std::vector<size_t> isolate_data_indices {
)";
  WriteVector(&ss,
              data->isolate_data_indices.data(),
              data->isolate_data_indices.size());
  ss << R"(};

const std::vector<size_t>* NodeMainInstance::GetIsolateDataIndices() {
  return &isolate_data_indices;
}

static const EnvSerializeInfo env_info )"
     << data->env_info << R"(;

const EnvSerializeInfo* NodeMainInstance::GetEnvSerializeInfo() {
  return &env_info;
}

}  // namespace node
)";

  return ss.str();
}

void SnapshotBuilder::Generate(SnapshotData* out,
                               const std::string& entry_file,
                               const std::vector<std::string> args,
                               const std::vector<std::string> exec_args) {
  Isolate* isolate = Isolate::Allocate();
  isolate->SetCaptureStackTraceForUncaughtExceptions(
      true, 10, v8::StackTrace::StackTraceOptions::kDetailed);
  per_process::v8_platform.Platform()->RegisterIsolate(isolate,
                                                       uv_default_loop());
  std::unique_ptr<NodeMainInstance> main_instance;
  std::string result;

  {
    const std::vector<intptr_t>& external_references =
        NodeMainInstance::CollectExternalReferences();
    SnapshotCreator creator(isolate, external_references.data());
    Environment* env;
    {
      main_instance =
          NodeMainInstance::Create(isolate,
                                   uv_default_loop(),
                                   per_process::v8_platform.Platform(),
                                   args,
                                   exec_args);

      HandleScope scope(isolate);
      creator.SetDefaultContext(Context::New(isolate));
      out->isolate_data_indices =
          main_instance->isolate_data()->Serialize(&creator);

      // Run the per-context scripts
      Local<Context> context;
      {
        TryCatch bootstrapCatch(isolate);
        context = NewContext(isolate);
        if (bootstrapCatch.HasCaught()) {
          PrintCaughtException(isolate, context, bootstrapCatch);
          abort();
        }
      }
      Context::Scope context_scope(context);

      // Create the environment
      env = new Environment(main_instance->isolate_data(),
                            context,
                            args,
                            exec_args,
                            nullptr,
                            node::EnvironmentFlags::kDefaultFlags,
                            {});

#if HAVE_INSPECTOR
     env->InitializeInspector({});
#endif
      // Run scripts in lib/internal/bootstrap/
      {
        TryCatch bootstrapCatch(isolate);
        MaybeLocal<Value> result = env->RunBootstrapping();
        if (bootstrapCatch.HasCaught()) {
          PrintCaughtException(isolate, context, bootstrapCatch);
        }
        result.ToLocalChecked();
      }

      // Run the entry point file
      if (!entry_file.empty()) {
        TryCatch bootstrapCatch(isolate);
        // TODO(joyee): we could use the result for something special, like
        // setting up initializers that should be invoked at snapshot
        // dehydration.
        MaybeLocal<Value> result =
            LoadEnvironment(env, StartExecutionCallback{});
        if (bootstrapCatch.HasCaught()) {
          PrintCaughtException(isolate, context, bootstrapCatch);
        }
        result.ToLocalChecked();
        // FIXME(joyee): right now running the loop in the snapshot builder
        // seems to intrudoces inconsistencies in JS land that need to be
        // synchronized again after snapshot restoration.
        int exit_code = SpinEventLoop(env).FromMaybe(1);
        CHECK_EQ(exit_code, 0);
      }

      if (per_process::enabled_debug_list.enabled(DebugCategory::MKSNAPSHOT)) {
        env->PrintAllBaseObjects();
        printf("Environment = %p\n", env);
      }

      // Serialize the native states
      out->env_info = env->Serialize(&creator);
      // Serialize the context
      size_t index = creator.AddContext(
          context, {SerializeNodeContextInternalFields, env});
      CHECK_EQ(index, NodeMainInstance::kNodeContextIndex);
    }

    // Must be out of HandleScope
    out->blob =
        creator.CreateBlob(SnapshotCreator::FunctionCodeHandling::kClear);

    // We must be able to rehash the blob when we restore it or otherwise
    // the hash seed would be fixed by V8, introducing a vulnerability.
    CHECK(out->blob.CanBeRehashed());

    // We cannot resurrect the handles from the snapshot, so make sure that
    // no handles are left open in the environment after the blob is created
    // (which should trigger a GC and close all handles that can be closed).
    if (!env->req_wrap_queue()->IsEmpty()
        || !env->handle_wrap_queue()->IsEmpty()
        || per_process::enabled_debug_list.enabled(DebugCategory::MKSNAPSHOT)) {
      PrintLibuvHandleInformation(env->event_loop(), stderr);
    }
    CHECK(env->req_wrap_queue()->IsEmpty());
    CHECK(env->handle_wrap_queue()->IsEmpty());

    // Must be done while the snapshot creator isolate is entered i.e. the
    // creator is still alive.
    FreeEnvironment(env);
    main_instance->Dispose();
  }

  per_process::v8_platform.Platform()->UnregisterIsolate(isolate);
}

std::string SnapshotBuilder::Generate(
    const std::string& entry_file,
    const std::vector<std::string> args,
    const std::vector<std::string> exec_args) {
  SnapshotData data;
  Generate(&data, entry_file, args, exec_args);
  std::string result = FormatBlob(&data);
  delete[] data.blob.data;
  return result;
}

SnapshotableObject::SnapshotableObject(Environment* env,
                                       Local<Object> wrap,
                                       EmbedderObjectType type)
    : BaseObject(env, wrap), type_(type) {
}

const char* SnapshotableObject::GetTypeNameChars() const {
  switch (type_) {
#define V(PropertyName, NativeTypeName)                                        \
  case EmbedderObjectType::k_##PropertyName: {                                 \
    return NativeTypeName::type_name.c_str();                                  \
  }
    SERIALIZABLE_OBJECT_TYPES(V)
#undef V
    default: { UNREACHABLE(); }
  }
}

bool IsSnapshotableType(FastStringKey key) {
#define V(PropertyName, NativeTypeName)                                        \
  if (key == NativeTypeName::type_name) {                                      \
    return true;                                                               \
  }
  SERIALIZABLE_OBJECT_TYPES(V)
#undef V

  return false;
}

void DeserializeNodeInternalFields(Local<Object> holder,
                                   int index,
                                   StartupData payload,
                                   void* env) {
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Deserialize internal field %d of %p, size=%d\n",
                     static_cast<int>(index),
                     (*holder),
                     static_cast<int>(payload.raw_size));
  if (payload.raw_size == 0) {
    holder->SetAlignedPointerInInternalField(index, nullptr);
    return;
  }

  Environment* env_ptr = static_cast<Environment*>(env);
  const InternalFieldInfo* info =
      reinterpret_cast<const InternalFieldInfo*>(payload.data);

  switch (info->type) {
#define V(PropertyName, NativeTypeName)                                        \
  case EmbedderObjectType::k_##PropertyName: {                                 \
    per_process::Debug(DebugCategory::MKSNAPSHOT,                              \
                       "Object %p is %s\n",                                    \
                       (*holder),                                              \
                       NativeTypeName::type_name.c_str());                     \
    env_ptr->EnqueueDeserializeRequest(                                        \
        NativeTypeName::Deserialize, holder, index, info->Copy());             \
    break;                                                                     \
  }
    SERIALIZABLE_OBJECT_TYPES(V)
#undef V
    default: { UNREACHABLE(); }
  }
}

StartupData SerializeNodeContextInternalFields(Local<Object> holder,
                                               int index,
                                               void* env) {
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Serialize internal field, index=%d, holder=%p\n",
                     static_cast<int>(index),
                     *holder);
  void* ptr = holder->GetAlignedPointerFromInternalField(BaseObject::kSlot);
  if (ptr == nullptr) {
    return StartupData{nullptr, 0};
  }

  DCHECK(static_cast<BaseObject*>(ptr)->is_snapshotable());
  SnapshotableObject* obj = static_cast<SnapshotableObject*>(ptr);
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Object %p is %s, ",
                     *holder,
                     obj->GetTypeNameChars());
  InternalFieldInfo* info = obj->Serialize(index);
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "payload size=%d\n",
                     static_cast<int>(info->length));
  return StartupData{reinterpret_cast<const char*>(info),
                     static_cast<int>(info->length)};
}

void SerializeBindingData(Environment* env,
                          SnapshotCreator* creator,
                          EnvSerializeInfo* info) {
  size_t i = 0;
  env->ForEachBindingData([&](FastStringKey key,
                              BaseObjectPtr<BaseObject> binding) {
    per_process::Debug(DebugCategory::MKSNAPSHOT,
                       "Serialize binding %i, %p, type=%s\n",
                       static_cast<int>(i),
                       *(binding->object()),
                       key.c_str());

    if (IsSnapshotableType(key)) {
      size_t index = creator->AddData(env->context(), binding->object());
      per_process::Debug(DebugCategory::MKSNAPSHOT,
                         "Serialized with index=%d\n",
                         static_cast<int>(index));
      info->bindings.push_back({key.c_str(), i, index});
      SnapshotableObject* ptr = static_cast<SnapshotableObject*>(binding.get());
      ptr->PrepareForSerialization(env->context(), creator);
    } else {
      UNREACHABLE();
    }

    i++;
  });
}

namespace mksnapshot {

static void CompileSnapshotMain(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  Local<String> source = args[0].As<String>();
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  std::string filename_s = std::string("node:snapshot_main");
  Local<String> filename =
      OneByteString(isolate, filename_s.c_str(), filename_s.size());
  ScriptOrigin origin(isolate, filename, 0, 0, true);
  // TODO(joyee): do we need all of these? Maybe we would want a less
  // internal version of them.
  std::vector<Local<String>> parameters = {
      FIXED_ONE_BYTE_STRING(isolate, "require")};
  ScriptCompiler::Source script_source(source, origin);
  Local<Function> fn;
  if (ScriptCompiler::CompileFunctionInContext(context,
                                               &script_source,
                                               parameters.size(),
                                               parameters.data(),
                                               0,
                                               nullptr,
                                               ScriptCompiler::kEagerCompile)
          .ToLocal(&fn)) {
    args.GetReturnValue().Set(fn);
  }
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = context->GetIsolate();
  env->SetMethod(target, "compileSnapshotMain", CompileSnapshotMain);
  target
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "cleanups"),
            v8::Array::New(isolate))
      .Check();
}

static void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(CompileSnapshotMain);
  registry->Register(MarkBootstrapComplete);
}
}  // namespace mksnapshot
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(mksnapshot, node::mksnapshot::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(mksnapshot,
                               node::mksnapshot::RegisterExternalReferences)
