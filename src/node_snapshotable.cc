
#include "node_snapshotable.h"
#include <iostream>
#include <sstream>
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_blob.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_file.h"
#include "node_internals.h"
#include "node_main_instance.h"
#include "node_metadata.h"
#include "node_native_module.h"
#include "node_process.h"
#include "node_snapshot_builder.h"
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
using v8::Object;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::SnapshotCreator;
using v8::StartupData;
using v8::String;
using v8::TryCatch;
using v8::Value;

const uint32_t SnapshotData::kMagic;

std::ostream& operator<<(std::ostream& output,
                         const native_module::CodeCacheInfo& info) {
  output << "<native_module::CodeCacheInfo id=" << info.id
         << ", size=" << info.data.size() << ">\n";
  return output;
}

std::ostream& operator<<(std::ostream& output,
                         const std::vector<native_module::CodeCacheInfo>& vec) {
  output << "{\n";
  for (const auto& info : vec) {
    output << info;
  }
  output << "}\n";
  return output;
}

std::ostream& operator<<(std::ostream& output,
                         const std::vector<uint8_t>& vec) {
  output << "{\n";
  for (const auto& i : vec) {
    output << i << ",";
  }
  output << "}";
  return output;
}

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
  std::string GetName() const {
#define TYPE_LIST(V)                                                           \
  V(native_module::CodeCacheInfo)                                              \
  V(PropInfo)                                                                  \
  V(std::string)

#define V(TypeName)                                                            \
  if (std::is_same_v<T, TypeName>) {                                           \
    return #TypeName;                                                          \
  }
    TYPE_LIST(V)
#undef V

    std::string name;
    if (std::is_arithmetic_v<T>) {
      if (!std::is_signed_v<T>) {
        name += "u";
      }
      name += std::is_integral_v<T> ? "int" : "float";
      name += std::to_string(sizeof(T) * 8);
      name += "_t";
    }
    return name;
  }

  FILE* f = nullptr;
  bool is_debug = false;
};

class FileReader : public FileIO {
 public:
  explicit FileReader(FILE* file) : FileIO(file) {}
  ~FileReader() {}

  // Helper for reading numeric types.
  template <typename T>
  T Read() {
    static_assert(std::is_arithmetic_v<T>, "Not an arithmetic type");
    T result;
    Read(&result, 1);
    return result;
  }

  // Layout of vectors:
  // [ 4/8 bytes ] count
  // [   ...     ] contents (count * size of individual elements)
  template <typename T>
  std::vector<T> ReadVector() {
    if (is_debug) {
      std::string name = GetName<T>();
      Debug("\nReadVector<%s>()(%d-byte)\n", name.c_str(), sizeof(T));
    }
    size_t count = static_cast<size_t>(Read<size_t>());
    if (count == 0) {
      return std::vector<T>();
    }
    if (is_debug) {
      Debug("Reading %d vector elements...\n", count);
    }
    std::vector<T> result = ReadVector<T>(count, std::is_arithmetic<T>{});
    if (is_debug) {
      std::string str = std::is_arithmetic_v<T> ? "" : ToStr(result);
      std::string name = GetName<T>();
      Debug("ReadVector<%s>() read %s\n", name.c_str(), str.c_str());
    }
    return result;
  }

  std::string ReadString() {
    size_t length = Read<size_t>();

    if (is_debug) {
      Debug("ReadString(), length=%d: ", length);
    }

    CHECK_GT(length, 0);  // There should be no empty strings.
    MallocedBuffer<char> buf(length + 1);
    size_t r = fread(buf.data, 1, length + 1, f);
    CHECK_EQ(r, length + 1);
    std::string result(buf.data, length);  // This creates a copy of buf.data.

    if (is_debug) {
      Debug("\"%s\", read %d bytes\n", result.c_str(), r);
    }

    read_total += r;
    return result;
  }

  size_t read_total = 0;

 private:
  // Helper for reading an array of numeric types.
  template <typename T>
  void Read(T* out, size_t count) {
    static_assert(std::is_arithmetic_v<T>, "Not an arithmetic type");
    DCHECK_GT(count, 0);  // Should not read contents for vectors of size 0.
    if (is_debug) {
      std::string name = GetName<T>();
      Debug("Read<%s>()(%d-byte), count=%d: ", name.c_str(), sizeof(T), count);
    }

    size_t r = fread(out, sizeof(T), count, f);
    CHECK_EQ(r, count);

    if (is_debug) {
      std::string str =
          "{ " + std::to_string(out[0]) + (count > 1 ? ", ... }" : " }");
      Debug("%s, read %d bytes\n", str.c_str(), r);
    }
    read_total += r;
  }

  // Helper for reading numeric vectors.
  template <typename Number>
  std::vector<Number> ReadVector(size_t count, std::true_type) {
    static_assert(std::is_arithmetic_v<Number>, "Not an arithmetic type");
    DCHECK_GT(count, 0);  // Should not read contents for vectors of size 0.
    std::vector<Number> result(count);
    Read(result.data(), count);
    return result;
  }

  // Helper for reading non-numeric vectors.
  template <typename T>
  std::vector<T> ReadVector(size_t count, std::false_type) {
    static_assert(!std::is_arithmetic_v<T>, "Arithmetic type");
    DCHECK_GT(count, 0);  // Should not read contents for vectors of size 0.
    std::vector<T> result;
    result.reserve(count);
    bool original_is_debug = is_debug;
    is_debug = original_is_debug && !std::is_same_v<T, std::string>;
    for (size_t i = 0; i < count; ++i) {
      if (is_debug) {
        Debug("\n[%d] ", i);
      }
      result.push_back(Read<T>());
    }
    is_debug = original_is_debug;

    return result;
  }
};

class FileWriter : public FileIO {
 public:
  explicit FileWriter(FILE* file) : FileIO(file) {}
  ~FileWriter() {}

  // Helper for writing numeric types.
  template <typename T>
  size_t Write(const T& data) {
    static_assert(std::is_arithmetic_v<T>, "Not an arithmetic type");
    return Write(&data, 1);
  }

  // Layout of vectors:
  // [ 4/8 bytes ] count
  // [   ...     ] contents (count * size of individual elements)
  template <typename T>
  size_t WriteVector(const std::vector<T>& data) {
    if (is_debug) {
      std::string str = std::is_arithmetic_v<T> ? "" : ToStr(data);
      std::string name = GetName<T>();
      Debug("\nWriteVector<%s>() (%d-byte), count=%d: %s\n",
            name.c_str(),
            sizeof(T),
            data.size(),
            str.c_str());
    }

    size_t written_total = Write<size_t>(data.size());
    if (data.size() == 0) {
      return written_total;
    }
    written_total += WriteVector<T>(data, std::is_arithmetic<T>{});

    if (is_debug) {
      std::string name = GetName<T>();
      Debug("WriteVector<%s>() wrote %d bytes\n", name.c_str(), written_total);
    }

    return written_total;
  }

  // The layout of a written string:
  // [  4/8 bytes     ] length
  // [ |length| bytes ] contents
  size_t WriteString(const std::string& data) {
    CHECK_GT(data.size(), 0);  // No empty strings should be written.
    size_t written_total = Write<size_t>(data.size());
    if (is_debug) {
      std::string str = ToStr(data);
      Debug("WriteString(), length=%d: \"%s\"\n", data.size(), data.c_str());
    }

    size_t r = fwrite(data.c_str(), 1, data.size() + 1, f);
    CHECK_EQ(r, data.size() + 1);
    written_total += r;

    if (is_debug) {
      Debug("WriteString() wrote %d bytes\n", written_total);
    }

    return written_total;
  }

 private:
  // Helper for writing an array of numeric types.
  template <typename T>
  size_t Write(const T* data, size_t count) {
    DCHECK_GT(count, 0);  // Should not write contents for vectors of size 0.
    if (is_debug) {
      std::string str =
          "{ " + std::to_string(data[0]) + (count > 1 ? ", ... }" : " }");
      std::string name = GetName<T>();
      Debug("Write<%s>() (%d-byte), count=%d: %s",
            name.c_str(),
            sizeof(T),
            count,
            str.c_str());
    }

    size_t r = fwrite(data, sizeof(T), count, f);
    CHECK_EQ(r, count);

    if (is_debug) {
      Debug(", wrote %d bytes\n", r);
    }
    return r;
  }

  // Helper for writing numeric vectors.
  template <typename Number>
  size_t WriteVector(const std::vector<Number>& data, std::true_type) {
    return Write(data.data(), data.size());
  }

  // Helper for writing non-numeric vectors.
  template <typename T>
  size_t WriteVector(const std::vector<T>& data, std::false_type) {
    DCHECK_GT(data.size(),
              0);  // Should not write contents for vectors of size 0.
    size_t written_total = 0;
    bool original_is_debug = is_debug;
    is_debug = original_is_debug && !std::is_same_v<T, std::string>;
    for (size_t i = 0; i < data.size(); ++i) {
      if (is_debug) {
        Debug("\n[%d] ", i);
      }
      written_total += Write<T>(data[i]);
    }
    is_debug = original_is_debug;

    return written_total;
  }
};

// Layout of serialized std::string:
// [  4/8 bytes     ] length
// [ |length| bytes ] contents
template <>
std::string FileReader::Read() {
  return ReadString();
}
template <>
size_t FileWriter::Write(const std::string& data) {
  return WriteString(data);
}

// Layout of v8::StartupData
// [  4/8 bytes       ] raw_size
// [ |raw_size| bytes ] contents
template <>
v8::StartupData FileReader::Read() {
  Debug("Read<v8::StartupData>()\n");

  int raw_size = Read<int>();
  Debug("size=%d\n", raw_size);

  CHECK_GT(raw_size, 0);  // There should be no startup data of size 0.
  // The data pointer of v8::StartupData would be deleted so it must be new'ed.
  std::unique_ptr<char> buf = std::unique_ptr<char>(new char[raw_size]);
  Read<char>(buf.get(), raw_size);

  return v8::StartupData{buf.release(), raw_size};
}

template <>
size_t FileWriter::Write(const v8::StartupData& data) {
  Debug("\nWrite<v8::StartupData>() size=%d\n", data.raw_size);

  CHECK_GT(data.raw_size, 0);  // There should be no startup data of size 0.
  size_t written_total = Write<int>(data.raw_size);
  written_total += Write<char>(data.data, static_cast<size_t>(data.raw_size));

  Debug("Write<v8::StartupData>() wrote %d bytes\n\n", written_total);
  return written_total;
}

// Layout of native_module::CodeCacheInfo
// [  4/8 bytes ]  length of the module id string
// [    ...     ]  |length| bytes of module id
// [  4/8 bytes ]  length of module code cache
// [    ...     ]  |length| bytes of module code cache
template <>
native_module::CodeCacheInfo FileReader::Read() {
  Debug("Read<native_module::CodeCacheInfo>()\n");

  native_module::CodeCacheInfo result{ReadString(), ReadVector<uint8_t>()};

  if (is_debug) {
    std::string str = ToStr(result);
    Debug("Read<native_module::CodeCacheInfo>() %s\n", str.c_str());
  }
  return result;
}

template <>
size_t FileWriter::Write(const native_module::CodeCacheInfo& data) {
  Debug("\nWrite<native_module::CodeCacheInfo>() id = %s"
        ", size=%d\n",
        data.id.c_str(),
        data.data.size());

  size_t written_total = WriteString(data.id);
  written_total += WriteVector<uint8_t>(data.data);

  Debug("Write<native_module::CodeCacheInfo>() wrote %d bytes\n",
        written_total);
  return written_total;
}

// Layout of PropInfo
// [ 4/8 bytes ]  length of the data name string
// [    ...    ]  |length| bytes of data name
// [  4 bytes  ]  index in the PropInfo vector
// [ 4/8 bytes ]  index in the snapshot blob, can be used with
//                GetDataFromSnapshotOnce().
template <>
PropInfo FileReader::Read() {
  Debug("Read<PropInfo>()\n");

  PropInfo result;
  result.name = ReadString();
  result.id = Read<uint32_t>();
  result.index = Read<SnapshotIndex>();

  if (is_debug) {
    std::string str = ToStr(result);
    Debug("Read<PropInfo>() %s\n", str.c_str());
  }

  return result;
}

template <>
size_t FileWriter::Write(const PropInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<PropInfo>() %s\n", str.c_str());
  }

  size_t written_total = WriteString(data.name);
  written_total += Write<uint32_t>(data.id);
  written_total += Write<SnapshotIndex>(data.index);

  Debug("Write<PropInfo>() wrote %d bytes\n", written_total);
  return written_total;
}

// Layout of AsyncHooks::SerializeInfo
// [ 4/8 bytes ]  snapshot index of async_ids_stack
// [ 4/8 bytes ]  snapshot index of fields
// [ 4/8 bytes ]  snapshot index of async_id_fields
// [ 4/8 bytes ]  snapshot index of js_execution_async_resources
// [ 4/8 bytes ]  length of native_execution_async_resources
// [   ...     ]  snapshot indices of each element in
//                native_execution_async_resources
template <>
AsyncHooks::SerializeInfo FileReader::Read() {
  Debug("Read<AsyncHooks::SerializeInfo>()\n");

  AsyncHooks::SerializeInfo result;
  result.async_ids_stack = Read<AliasedBufferIndex>();
  result.fields = Read<AliasedBufferIndex>();
  result.async_id_fields = Read<AliasedBufferIndex>();
  result.js_execution_async_resources = Read<SnapshotIndex>();
  result.native_execution_async_resources = ReadVector<SnapshotIndex>();

  if (is_debug) {
    std::string str = ToStr(result);
    Debug("Read<AsyncHooks::SerializeInfo>() %s\n", str.c_str());
  }

  return result;
}
template <>
size_t FileWriter::Write(const AsyncHooks::SerializeInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<AsyncHooks::SerializeInfo>() %s\n", str.c_str());
  }

  size_t written_total = Write<AliasedBufferIndex>(data.async_ids_stack);
  written_total += Write<AliasedBufferIndex>(data.fields);
  written_total += Write<AliasedBufferIndex>(data.async_id_fields);
  written_total += Write<SnapshotIndex>(data.js_execution_async_resources);
  written_total +=
      WriteVector<SnapshotIndex>(data.native_execution_async_resources);

  Debug("Write<AsyncHooks::SerializeInfo>() wrote %d bytes\n", written_total);
  return written_total;
}

// Layout of TickInfo::SerializeInfo
// [ 4/8 bytes ]  snapshot index of fields
template <>
TickInfo::SerializeInfo FileReader::Read() {
  Debug("Read<TickInfo::SerializeInfo>()\n");

  TickInfo::SerializeInfo result;
  result.fields = Read<AliasedBufferIndex>();

  if (is_debug) {
    std::string str = ToStr(result);
    Debug("Read<TickInfo::SerializeInfo>() %s\n", str.c_str());
  }

  return result;
}

template <>
size_t FileWriter::Write(const TickInfo::SerializeInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<TickInfo::SerializeInfo>() %s\n", str.c_str());
  }

  size_t written_total = Write<AliasedBufferIndex>(data.fields);

  Debug("Write<TickInfo::SerializeInfo>() wrote %d bytes\n", written_total);
  return written_total;
}

// Layout of TickInfo::SerializeInfo
// [ 4/8 bytes ]  snapshot index of fields
template <>
ImmediateInfo::SerializeInfo FileReader::Read() {
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Read<ImmediateInfo::SerializeInfo>()\n");

  ImmediateInfo::SerializeInfo result;
  result.fields = Read<AliasedBufferIndex>();
  if (is_debug) {
    std::string str = ToStr(result);
    Debug("Read<ImmediateInfo::SerializeInfo>() %s\n", str.c_str());
  }
  return result;
}

template <>
size_t FileWriter::Write(const ImmediateInfo::SerializeInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<ImmeidateInfo::SerializeInfo>() %s\n", str.c_str());
  }

  size_t written_total = Write<AliasedBufferIndex>(data.fields);

  Debug("Write<ImmeidateInfo::SerializeInfo>() wrote %d bytes\n",
        written_total);
  return written_total;
}

// Layout of PerformanceState::SerializeInfo
// [ 4/8 bytes ]  snapshot index of root
// [ 4/8 bytes ]  snapshot index of milestones
// [ 4/8 bytes ]  snapshot index of observers
template <>
performance::PerformanceState::SerializeInfo FileReader::Read() {
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Read<PerformanceState::SerializeInfo>()\n");

  performance::PerformanceState::SerializeInfo result;
  result.root = Read<AliasedBufferIndex>();
  result.milestones = Read<AliasedBufferIndex>();
  result.observers = Read<AliasedBufferIndex>();
  if (is_debug) {
    std::string str = ToStr(result);
    Debug("Read<PerformanceState::SerializeInfo>() %s\n", str.c_str());
  }
  return result;
}

template <>
size_t FileWriter::Write(
    const performance::PerformanceState::SerializeInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<PerformanceState::SerializeInfo>() %s\n", str.c_str());
  }

  size_t written_total = Write<AliasedBufferIndex>(data.root);
  written_total += Write<AliasedBufferIndex>(data.milestones);
  written_total += Write<AliasedBufferIndex>(data.observers);

  Debug("Write<PerformanceState::SerializeInfo>() wrote %d bytes\n",
        written_total);
  return written_total;
}

// Layout of IsolateDataSerializeInfo
// [ 4/8 bytes ]  length of primitive_values vector
// [    ...    ]  |length| of primitive_values indices
// [ 4/8 bytes ]  length of template_values vector
// [    ...    ]  |length| of PropInfo data
template <>
IsolateDataSerializeInfo FileReader::Read() {
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Read<IsolateDataSerializeInfo>()\n");

  IsolateDataSerializeInfo result;
  result.primitive_values = ReadVector<SnapshotIndex>();
  result.template_values = ReadVector<PropInfo>();
  if (is_debug) {
    std::string str = ToStr(result);
    Debug("Read<IsolateDataSerializeInfo>() %s\n", str.c_str());
  }
  return result;
}

template <>
size_t FileWriter::Write(const IsolateDataSerializeInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("Write<IsolateDataSerializeInfo>() %s\n", str.c_str());
  }

  size_t written_total = WriteVector<SnapshotIndex>(data.primitive_values);
  written_total += WriteVector<PropInfo>(data.template_values);

  Debug("Write<IsolateDataSerializeInfo>() wrote %d bytes\n", written_total);
  return written_total;
}

template <>
EnvSerializeInfo FileReader::Read() {
  per_process::Debug(DebugCategory::MKSNAPSHOT, "Read<EnvSerializeInfo>()\n");
  EnvSerializeInfo result;
  result.bindings = ReadVector<PropInfo>();
  result.native_modules = ReadVector<std::string>();
  result.async_hooks = Read<AsyncHooks::SerializeInfo>();
  result.tick_info = Read<TickInfo::SerializeInfo>();
  result.immediate_info = Read<ImmediateInfo::SerializeInfo>();
  result.performance_state =
      Read<performance::PerformanceState::SerializeInfo>();
  result.exiting = Read<AliasedBufferIndex>();
  result.stream_base_state = Read<AliasedBufferIndex>();
  result.should_abort_on_uncaught_toggle = Read<AliasedBufferIndex>();
  result.persistent_values = ReadVector<PropInfo>();
  result.context = Read<SnapshotIndex>();
  return result;
}

template <>
size_t FileWriter::Write(const EnvSerializeInfo& data) {
  if (is_debug) {
    std::string str = ToStr(data);
    Debug("\nWrite<EnvSerializeInfo>() %s\n", str.c_str());
  }

  // Use += here to ensure order of evaluation.
  size_t written_total = WriteVector<PropInfo>(data.bindings);
  written_total += WriteVector<std::string>(data.native_modules);
  written_total += Write<AsyncHooks::SerializeInfo>(data.async_hooks);
  written_total += Write<TickInfo::SerializeInfo>(data.tick_info);
  written_total += Write<ImmediateInfo::SerializeInfo>(data.immediate_info);
  written_total += Write<performance::PerformanceState::SerializeInfo>(
      data.performance_state);
  written_total += Write<AliasedBufferIndex>(data.exiting);
  written_total += Write<AliasedBufferIndex>(data.stream_base_state);
  written_total +=
      Write<AliasedBufferIndex>(data.should_abort_on_uncaught_toggle);
  written_total += WriteVector<PropInfo>(data.persistent_values);
  written_total += Write<SnapshotIndex>(data.context);

  Debug("Write<EnvSerializeInfo>() wrote %d bytes\n", written_total);
  return written_total;
}

// Layout of the snapshot blob
// [   4 bytes    ]  kMagic
// [   4/8 bytes  ]  length of Node.js version string
// [    ...       ]  contents of Node.js version string
// [   4/8 bytes  ]  length of Node.js arch string
// [    ...       ]  contents of Node.js arch string
// [    ...       ]  v8_snapshot_blob_data from SnapshotCreator::CreateBlob()
// [    ...       ]  isolate_data_info
// [    ...       ]  env_info
// [    ...       ]  code_cache

void SnapshotData::ToBlob(FILE* out) const {
  FileWriter w(out);
  w.Debug("SnapshotData::ToBlob()\n");

  size_t written_total = 0;
  // Metadata
  w.Debug("Write magic %" PRIx32 "\n", kMagic);
  written_total += w.Write<uint32_t>(kMagic);
  w.Debug("Write version %s\n", NODE_VERSION);
  written_total += w.WriteString(NODE_VERSION);
  w.Debug("Write arch %s\n", NODE_ARCH);
  written_total += w.WriteString(NODE_ARCH);

  written_total += w.Write<v8::StartupData>(v8_snapshot_blob_data);
  w.Debug("Write isolate_data_indices\n");
  written_total += w.Write<IsolateDataSerializeInfo>(isolate_data_info);
  written_total += w.Write<EnvSerializeInfo>(env_info);
  w.Debug("Write code_cache\n");
  written_total += w.WriteVector<native_module::CodeCacheInfo>(code_cache);
  w.Debug("SnapshotData::ToBlob() Wrote %d bytes\n", written_total);
}

void SnapshotData::FromBlob(SnapshotData* out, FILE* in) {
  FileReader r(in);
  r.Debug("SnapshotData::FromBlob()\n");

  // Metadata
  uint32_t magic = r.Read<uint32_t>();
  r.Debug("Read magic %" PRIx64 "\n", magic);
  CHECK_EQ(magic, kMagic);
  std::string version = r.ReadString();
  r.Debug("Read version %s\n", version.c_str());
  CHECK_EQ(version, NODE_VERSION);
  std::string arch = r.ReadString();
  r.Debug("Read arch %s\n", arch.c_str());
  CHECK_EQ(arch, NODE_ARCH);

  DCHECK_EQ(out->data_ownership, SnapshotData::DataOwnership::kOwned);
  out->v8_snapshot_blob_data = r.Read<v8::StartupData>();
  r.Debug("Read isolate_data_info\n");
  out->isolate_data_info = r.Read<IsolateDataSerializeInfo>();
  out->env_info = r.Read<EnvSerializeInfo>();
  r.Debug("Read code_cache\n");
  out->code_cache = r.ReadVector<native_module::CodeCacheInfo>();

  r.Debug("SnapshotData::FromBlob() read %d bytes\n", r.read_total);
}

SnapshotData::~SnapshotData() {
  if (data_ownership == DataOwnership::kOwned &&
      v8_snapshot_blob_data.data != nullptr) {
    delete[] v8_snapshot_blob_data.data;
  }
}

template <typename T>
void WriteVector(std::ostream* ss, const T* vec, size_t size) {
  for (size_t i = 0; i < size; i++) {
    *ss << std::to_string(vec[i]) << (i == size - 1 ? '\n' : ',');
  }
}

static std::string GetCodeCacheDefName(const std::string& id) {
  char buf[64] = {0};
  size_t size = id.size();
  CHECK_LT(size, sizeof(buf));
  for (size_t i = 0; i < size; ++i) {
    char ch = id[i];
    buf[i] = (ch == '-' || ch == '/') ? '_' : ch;
  }
  return std::string(buf) + std::string("_cache_data");
}

static std::string FormatSize(size_t size) {
  char buf[64] = {0};
  if (size < 1024) {
    snprintf(buf, sizeof(buf), "%.2fB", static_cast<double>(size));
  } else if (size < 1024 * 1024) {
    snprintf(buf, sizeof(buf), "%.2fKB", static_cast<double>(size / 1024));
  } else {
    snprintf(
        buf, sizeof(buf), "%.2fMB", static_cast<double>(size / 1024 / 1024));
  }
  return buf;
}

static void WriteStaticCodeCacheData(std::ostream* ss,
                                     const native_module::CodeCacheInfo& info) {
  *ss << "static const uint8_t " << GetCodeCacheDefName(info.id) << "[] = {\n";
  WriteVector(ss, info.data.data(), info.data.size());
  *ss << "};";
}

static void WriteCodeCacheInitializer(std::ostream* ss, const std::string& id) {
  std::string def_name = GetCodeCacheDefName(id);
  *ss << "    { \"" << id << "\",\n";
  *ss << "      {" << def_name << ",\n";
  *ss << "       " << def_name << " + arraysize(" << def_name << "),\n";
  *ss << "      }\n";
  *ss << "    },\n";
}

void FormatBlob(std::ostream& ss, SnapshotData* data) {
  ss << R"(#include <cstddef>
#include "env.h"
#include "node_snapshot_builder.h"
#include "v8.h"

// This file is generated by tools/snapshot. Do not edit.

namespace node {

static const char v8_snapshot_blob_data[] = {
)";
  WriteVector(&ss,
              data->v8_snapshot_blob_data.data,
              data->v8_snapshot_blob_data.raw_size);
  ss << R"(};

static const int v8_snapshot_blob_size = )"
     << data->v8_snapshot_blob_data.raw_size << ";";

  // Windows can't deal with too many large vector initializers.
  // Store the data into static arrays first.
  for (const auto& item : data->code_cache) {
    WriteStaticCodeCacheData(&ss, item);
  }

  ss << R"(SnapshotData snapshot_data {
  // -- data_ownership begins --
  SnapshotData::DataOwnership::kNotOwned,
  // -- data_ownership ends --
  // -- v8_snapshot_blob_data begins --
  { v8_snapshot_blob_data, v8_snapshot_blob_size },
  // -- v8_snapshot_blob_data ends --
  // -- isolate_data_indices begins --
)" << data->isolate_data_info
     << R"(
  // -- isolate_data_indices ends --
  ,
  // -- env_info begins --
)" << data->env_info
     << R"(
  // -- env_info ends --
  ,
  // -- code_cache begins --
  {)";
  for (const auto& item : data->code_cache) {
    WriteCodeCacheInitializer(&ss, item.id);
  }
  ss << R"(
  }
  // -- code_cache ends --
};

const SnapshotData* SnapshotBuilder::GetEmbeddedSnapshotData() {
  Mutex::ScopedLock lock(snapshot_data_mutex_);
  return &snapshot_data;
}
}  // namespace node
)";
}

Mutex SnapshotBuilder::snapshot_data_mutex_;

const std::vector<intptr_t>& SnapshotBuilder::CollectExternalReferences() {
  static auto registry = std::make_unique<ExternalReferenceRegistry>();
  return registry->external_references();
}

void SnapshotBuilder::InitializeIsolateParams(const SnapshotData* data,
                                              Isolate::CreateParams* params) {
  params->external_references = CollectExternalReferences().data();
  params->snapshot_blob =
      const_cast<v8::StartupData*>(&(data->v8_snapshot_blob_data));
}

// TODO(joyeecheung): share these exit code constants across the code base.
constexpr int UNCAUGHT_EXCEPTION_ERROR = 1;
constexpr int BOOTSTRAP_ERROR = 10;
constexpr int SNAPSHOT_ERROR = 14;

int SnapshotBuilder::Generate(SnapshotData* out,
                              const std::vector<std::string> args,
                              const std::vector<std::string> exec_args) {
  const std::vector<intptr_t>& external_references =
      CollectExternalReferences();
  Isolate* isolate = Isolate::Allocate();
  // Must be done before the SnapshotCreator creation so  that the
  // memory reducer can be initialized.
  per_process::v8_platform.Platform()->RegisterIsolate(isolate,
                                                       uv_default_loop());

  SnapshotCreator creator(isolate, external_references.data());

  isolate->SetCaptureStackTraceForUncaughtExceptions(
      true, 10, v8::StackTrace::StackTraceOptions::kDetailed);

  Environment* env = nullptr;
  std::unique_ptr<NodeMainInstance> main_instance =
      NodeMainInstance::Create(isolate,
                               uv_default_loop(),
                               per_process::v8_platform.Platform(),
                               args,
                               exec_args);

  // The cleanups should be done in case of an early exit due to errors.
  auto cleanup = OnScopeLeave([&]() {
    // Must be done while the snapshot creator isolate is entered i.e. the
    // creator is still alive. The snapshot creator destructor will destroy
    // the isolate.
    if (env != nullptr) {
      FreeEnvironment(env);
    }
    main_instance->Dispose();
    per_process::v8_platform.Platform()->UnregisterIsolate(isolate);
  });

  {
    HandleScope scope(isolate);
    TryCatch bootstrapCatch(isolate);

    auto print_Exception = OnScopeLeave([&]() {
      if (bootstrapCatch.HasCaught()) {
        PrintCaughtException(
            isolate, isolate->GetCurrentContext(), bootstrapCatch);
      }
    });

    // The default context with only things created by V8.
    Local<Context> default_context = Context::New(isolate);

    // The Node.js-specific context with primodials, can be used by workers
    // TODO(joyeecheung): investigate if this can be used by vm contexts
    // without breaking compatibility.
    Local<Context> base_context = NewContext(isolate);
    if (base_context.IsEmpty()) {
      return BOOTSTRAP_ERROR;
    }

    Local<Context> main_context = NewContext(isolate);
    if (main_context.IsEmpty()) {
      return BOOTSTRAP_ERROR;
    }
    // Initialize the main instance context.
    {
      Context::Scope context_scope(main_context);

      // Create the environment.
      env = new Environment(main_instance->isolate_data(),
                            main_context,
                            args,
                            exec_args,
                            nullptr,
                            node::EnvironmentFlags::kDefaultFlags,
                            {});

      // Run scripts in lib/internal/bootstrap/
      if (env->RunBootstrapping().IsEmpty()) {
        return BOOTSTRAP_ERROR;
      }
      // If --build-snapshot is true, lib/internal/main/mksnapshot.js would be
      // loaded via LoadEnvironment() to execute process.argv[1] as the entry
      // point (we currently only support this kind of entry point, but we
      // could also explore snapshotting other kinds of execution modes
      // in the future).
      if (per_process::cli_options->build_snapshot) {
#if HAVE_INSPECTOR
        // TODO(joyeecheung): move this before RunBootstrapping().
        env->InitializeInspector({});
#endif
        if (LoadEnvironment(env, StartExecutionCallback{}).IsEmpty()) {
          return UNCAUGHT_EXCEPTION_ERROR;
        }
        // FIXME(joyeecheung): right now running the loop in the snapshot
        // builder seems to introduces inconsistencies in JS land that need to
        // be synchronized again after snapshot restoration.
        int exit_code = SpinEventLoop(env).FromMaybe(UNCAUGHT_EXCEPTION_ERROR);
        if (exit_code != 0) {
          return exit_code;
        }
      }

      if (per_process::enabled_debug_list.enabled(DebugCategory::MKSNAPSHOT)) {
        env->PrintAllBaseObjects();
        printf("Environment = %p\n", env);
      }

      // Serialize the native states
      out->isolate_data_info =
          main_instance->isolate_data()->Serialize(&creator);
      out->env_info = env->Serialize(&creator);

#ifdef NODE_USE_NODE_CODE_CACHE
      // Regenerate all the code cache.
      if (!native_module::NativeModuleLoader::CompileAllBuiltins(
              main_context)) {
        return UNCAUGHT_EXCEPTION_ERROR;
      }
      native_module::NativeModuleLoader::CopyCodeCache(&(out->code_cache));
      for (const auto& item : out->code_cache) {
        std::string size_str = FormatSize(item.data.size());
        per_process::Debug(DebugCategory::MKSNAPSHOT,
                           "Generated code cache for %d: %s\n",
                           item.id.c_str(),
                           size_str.c_str());
      }
#endif
    }

    // Global handles to the contexts can't be disposed before the
    // blob is created. So initialize all the contexts before adding them.
    // TODO(joyeecheung): figure out how to remove this restriction.
    creator.SetDefaultContext(default_context);
    size_t index = creator.AddContext(base_context);
    CHECK_EQ(index, SnapshotData::kNodeBaseContextIndex);
    index = creator.AddContext(main_context,
                               {SerializeNodeContextInternalFields, env});
    CHECK_EQ(index, SnapshotData::kNodeMainContextIndex);
  }

  // Must be out of HandleScope
  out->v8_snapshot_blob_data =
      creator.CreateBlob(SnapshotCreator::FunctionCodeHandling::kClear);

  // We must be able to rehash the blob when we restore it or otherwise
  // the hash seed would be fixed by V8, introducing a vulnerability.
  if (!out->v8_snapshot_blob_data.CanBeRehashed()) {
    return SNAPSHOT_ERROR;
  }

  // We cannot resurrect the handles from the snapshot, so make sure that
  // no handles are left open in the environment after the blob is created
  // (which should trigger a GC and close all handles that can be closed).
  bool queues_are_empty =
      env->req_wrap_queue()->IsEmpty() && env->handle_wrap_queue()->IsEmpty();
  if (!queues_are_empty ||
      per_process::enabled_debug_list.enabled(DebugCategory::MKSNAPSHOT)) {
    PrintLibuvHandleInformation(env->event_loop(), stderr);
  }
  if (!queues_are_empty) {
    return SNAPSHOT_ERROR;
  }
  return 0;
}

int SnapshotBuilder::Generate(std::ostream& out,
                              const std::vector<std::string> args,
                              const std::vector<std::string> exec_args) {
  SnapshotData data;
  int exit_code = Generate(&data, args, exec_args);
  if (exit_code != 0) {
    return exit_code;
  }
  FormatBlob(out, &data);
  return exit_code;
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
  if (payload.raw_size == 0) {
    holder->SetAlignedPointerInInternalField(index, nullptr);
    return;
  }
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Deserialize internal field %d of %p, size=%d\n",
                     static_cast<int>(index),
                     (*holder),
                     static_cast<int>(payload.raw_size));
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
  void* ptr = holder->GetAlignedPointerFromInternalField(BaseObject::kSlot);
  if (ptr == nullptr) {
    return StartupData{nullptr, 0};
  }
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Serialize internal field, index=%d, holder=%p\n",
                     static_cast<int>(index),
                     *holder);
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
  uint32_t i = 0;
  env->ForEachBindingData([&](FastStringKey key,
                              BaseObjectPtr<BaseObject> binding) {
    per_process::Debug(DebugCategory::MKSNAPSHOT,
                       "Serialize binding %i, %p, type=%s\n",
                       static_cast<int>(i),
                       *(binding->object()),
                       key.c_str());

    if (IsSnapshotableType(key)) {
      SnapshotIndex index = creator->AddData(env->context(), binding->object());
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

void CompileSerializeMain(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  Local<String> filename = args[0].As<String>();
  Local<String> source = args[1].As<String>();
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  ScriptOrigin origin(isolate, filename, 0, 0, true);
  // TODO(joyeecheung): do we need all of these? Maybe we would want a less
  // internal version of them.
  std::vector<Local<String>> parameters = {
      FIXED_ONE_BYTE_STRING(isolate, "require"),
      FIXED_ONE_BYTE_STRING(isolate, "__filename"),
      FIXED_ONE_BYTE_STRING(isolate, "__dirname"),
  };
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

void SetSerializeCallback(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(env->snapshot_serialize_callback().IsEmpty());
  CHECK(args[0]->IsFunction());
  env->set_snapshot_serialize_callback(args[0].As<Function>());
}

void SetDeserializeCallback(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(env->snapshot_deserialize_callback().IsEmpty());
  CHECK(args[0]->IsFunction());
  env->set_snapshot_deserialize_callback(args[0].As<Function>());
}

void SetDeserializeMainFunction(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(env->snapshot_deserialize_main().IsEmpty());
  CHECK(args[0]->IsFunction());
  env->set_snapshot_deserialize_main(args[0].As<Function>());
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethod(context, target, "compileSerializeMain", CompileSerializeMain);
  SetMethod(context, target, "setSerializeCallback", SetSerializeCallback);
  SetMethod(context, target, "setDeserializeCallback", SetDeserializeCallback);
  SetMethod(context,
            target,
            "setDeserializeMainFunction",
            SetDeserializeMainFunction);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(CompileSerializeMain);
  registry->Register(SetSerializeCallback);
  registry->Register(SetDeserializeCallback);
  registry->Register(SetDeserializeMainFunction);
}
}  // namespace mksnapshot
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(mksnapshot, node::mksnapshot::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(mksnapshot,
                               node::mksnapshot::RegisterExternalReferences)
