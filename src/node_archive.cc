#include <utility>
#include <vector>

#include "async_wrap.h"
#include "env.h"
#include "env-inl.h"
#include "node_binding.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node.h"
#include "util.h"
#include "zip.h"
#include "v8.h"

#define CHECK_ZIP_ARCHIVE_ERR(zip, action, expr)                                                        \
  do {                                                                                                  \
    if (UNLIKELY(!(expr))) {                                                                            \
      THROW_ERR_LIBZIP_ERROR(env, "Zip " action " failed: %s", zip_error_strerror(zip_get_error(zip))); \
      return;                                                                                           \
    }                                                                                                   \
  } while (0)

#define CHECK_ZIP_FILE_ERR(file, action, expr)                                                                \
  do {                                                                                                        \
    if (UNLIKELY(!(expr))) {                                                                                  \
      THROW_ERR_LIBZIP_ERROR(env, "Zip " action " failed: %s", zip_error_strerror(zip_file_get_error(file))); \
      return;                                                                                                 \
    }                                                                                                         \
  } while (0)

namespace node {

namespace {

static char const emptyZipArchive[] = {
  0x50, 0x4B, 0x05, 0x06,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00
};

struct ZipCompContext {
  zip_uint64_t uncompressed_size;
  zip_uint32_t crc;
  zip_uint32_t compression_method;

  static zip_int64_t layer_callback(zip_source_t* src, void *ud, void* data, zip_uint64_t length, zip_source_cmd_t command) {
    ZipCompContext* ctx = static_cast<ZipCompContext*>(ud);

    switch (command) {
    case ZIP_SOURCE_FREE:
      delete ctx;
      return 0;

    case ZIP_SOURCE_STAT: {
      zip_stat_t *st = static_cast<zip_stat_t*>(data);
      if (st->valid & ZIP_STAT_SIZE) {
          st->comp_size = st->size;
          st->valid |= ZIP_STAT_COMP_SIZE;
      }

      st->size = ctx->uncompressed_size;
      st->crc = ctx->crc;
      st->comp_method = ctx->compression_method;

      st->valid |= ZIP_STAT_COMP_METHOD | ZIP_STAT_SIZE | ZIP_STAT_CRC;

      return 0;
    }

    default:
      return zip_source_pass_to_lower_layer(src, data, length, command);
    }
  }
};

class ZipArchive final : public BaseObject {
 public:
  enum InternalFields {
    kInternalFieldCount = BaseObject::kInternalFieldCount,
  };

  ZipArchive(Environment* env, v8::Local<v8::Object> obj, std::vector<char>&& buf);
  ~ZipArchive();

  void MemoryInfo(MemoryTracker* tracker) const override;

  static ZipArchive* CheckZip(const v8::FunctionCallbackInfo<v8::Value>& args);
  static std::pair<ZipArchive*, zip_uint32_t> CheckZipEntry(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void GetEntries(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddDirectory(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddEntry(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DeleteEntry(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReadEntry(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StatEntry(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RestatEntry(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Digest(const v8::FunctionCallbackInfo<v8::Value>& args);

  int GetFileType(zip_uint64_t index) const;

  std::vector<char> buf_;
  zip_error_t error_;
  zip_source_t* source_ = nullptr;
  zip_t* zip_ = nullptr;
  uint64_t mem_usage_ = 0;

  SET_MEMORY_INFO_NAME(ZipArchive)
  SET_SELF_SIZE(ZipArchive)
};

ZipArchive::ZipArchive(Environment* env, v8::Local<v8::Object> obj, std::vector<char>&& buf)
    : BaseObject(env, obj)
    , buf_(std::move(buf)) {
  MakeWeak();

  mem_usage_ += buf.size();

  zip_error_init(&error_);

  source_ = zip_source_buffer_create(buf_.data(), buf_.size(), 0, &error_);
  CHECK_NOT_NULL(source_);
  zip_source_keep(source_);

  zip_ = zip_open_from_source(source_, 0, &error_);
  if (!zip_) {
    char const* err = zip_error_strerror(&error_);
    THROW_ERR_ZIP_OPENING_FAILED(env, "Zip opening failed: %s", err);
  }
}

ZipArchive::~ZipArchive() {
  if (zip_) {
    zip_discard(zip_);
    zip_ = nullptr;
  }

  if (source_) {
    zip_source_free(source_);
    source_ = nullptr;
  }

  zip_error_fini(&error_);
}

void ZipArchive::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("libzip_memory", mem_usage_);
}

void ZipArchive::GetEntries(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  ZipArchive* zip = ZipArchive::CheckZip(args);

  const int argc = args.Length();
  CHECK_EQ(argc, 1);

  CHECK(args[0]->IsBoolean());
  bool with_file_types = args[0]->IsTrue();

  zip_int64_t num_entries = zip_get_num_entries(zip->zip_, 0);

  std::vector<v8::Local<v8::Value>> entries;
  entries.resize(num_entries);

  for (zip_int64_t t = 0; t < num_entries; ++t) {
    char const* entry_name = zip_get_name(zip->zip_, t, 0);
    CHECK_NOT_NULL(entry_name);

    v8::Local<v8::Value> entry_items[] = {
      v8::String::NewFromUtf8(isolate, entry_name).ToLocalChecked(),
      v8::Integer::New(isolate, with_file_types ? zip->GetFileType(t) : 0)
    };

    entries[t] = v8::Array::New(isolate, &entry_items[0], arraysize(entry_items));
  }

  args.GetReturnValue().Set(v8::Array::New(isolate, &entries[0], entries.size()));
}

void ZipArchive::AddDirectory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  ZipArchive* zip = ZipArchive::CheckZip(args);

  const int argc = args.Length();
  CHECK_EQ(argc, 1);

  CHECK(args[0]->IsString());
  node::Utf8Value path(isolate, args[0]);

  zip_int64_t dir_index = zip_dir_add(zip->zip_, *path, 0);
  CHECK_GE(dir_index, 0);
  CHECK_LE(dir_index, std::numeric_limits<uint32_t>::max());

  args.GetReturnValue().Set(static_cast<uint32_t>(dir_index));
}

void ZipArchive::AddEntry(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  ZipArchive* zip = ZipArchive::CheckZip(args);

  const int argc = args.Length();
  CHECK_EQ(argc, 5);

  CHECK(args[0]->IsString());
  node::Utf8Value path(isolate, args[0]);

  CHECK(IsSafeJsInt(args[1]));
  zip_int32_t comp_method = args[1].As<v8::Integer>()->Value();

  ArrayBufferViewContents<char> buffer_view(args[2]);
  char const* buffer_data = buffer_view.data();
  size_t buffer_length = buffer_view.length();

  char* buffer_copy = Malloc(buffer_length);
  memcpy(buffer_copy, buffer_data, buffer_length);

  zip_source_t* file_source = zip_source_buffer(zip->zip_, buffer_copy, buffer_length, 1);
  CHECK_NOT_NULL(file_source);

  CHECK(IsSafeJsInt(args[3]));
  zip_uint32_t crc = args[3].As<v8::Integer>()->Value();

  CHECK(IsSafeJsInt(args[4]));
  zip_uint32_t size = args[4].As<v8::Integer>()->Value();

  ZipCompContext* zip_comp_ctx = new ZipCompContext();
  zip_comp_ctx->compression_method = comp_method;
  zip_comp_ctx->uncompressed_size = size;
  zip_comp_ctx->crc = crc;

  zip_source_t* file_comp_source = zip_source_layered_create(file_source, &ZipCompContext::layer_callback, zip_comp_ctx, &zip->error_);
  CHECK_NOT_NULL(file_comp_source);

  zip_int64_t file_index = zip_file_add(zip->zip_, *path, file_comp_source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
  CHECK_GE(file_index, 0);
  CHECK_LE(file_index, std::numeric_limits<uint32_t>::max());

  zip->mem_usage_ += buffer_length;

  args.GetReturnValue().Set(static_cast<uint32_t>(file_index));
}

ZipArchive* ZipArchive::CheckZip(const v8::FunctionCallbackInfo<v8::Value>& args) {
  ZipArchive* zip;

  CHECK_NOT_NULL(zip = Unwrap<ZipArchive>(args.Holder()));
  CHECK_NOT_NULL(zip->zip_);
  CHECK_NOT_NULL(zip->source_);

  return zip;
}

std::pair<ZipArchive*, zip_uint32_t> ZipArchive::CheckZipEntry(const v8::FunctionCallbackInfo<v8::Value>& args) {
  ZipArchive* zip = ZipArchive::CheckZip(args);

  const int argc = args.Length();
  CHECK_GE(argc, 1);

  CHECK(IsSafeJsInt(args[0]));
  zip_uint32_t index = args[0].As<v8::Integer>()->Value();

  return std::make_pair(zip, index);
}

void ZipArchive::DeleteEntry(const v8::FunctionCallbackInfo<v8::Value>& args) {
  auto [zip, index] = ZipArchive::CheckZipEntry(args);

  CHECK_EQ(zip_delete(zip->zip_, index), 0);
}

void ZipArchive::ReadEntry(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);

  auto [zip, index] = ZipArchive::CheckZipEntry(args);

  zip_stat_t file_stat;
  CHECK_EQ(zip_stat_index(zip->zip_, index, 0, &file_stat), 0);
  CHECK_LE(file_stat.comp_size, ZIP_INT64_MAX);
  CHECK_LE(file_stat.comp_size, std::numeric_limits<size_t>::max());

  char* data = Malloc(file_stat.comp_size);

  zip_file_t* file;
  CHECK_NOT_NULL(file = zip_fopen_index(zip->zip_, index, ZIP_FL_COMPRESSED));

  zip_int64_t read_length = zip_fread(file, data, file_stat.comp_size);
  CHECK_ZIP_FILE_ERR(file, "fread", read_length >= 0);
  CHECK_EQ(read_length, static_cast<zip_int64_t>(file_stat.comp_size));

  CHECK_EQ(zip_fclose(file), 0);

  v8::Local<v8::Value> result[] = {
    v8::Integer::NewFromUnsigned(isolate, file_stat.comp_method),
    Buffer::New(isolate, data, read_length).ToLocalChecked(),
  };

  args.GetReturnValue().Set(v8::Array::New(isolate, &result[0], arraysize(result)));
}

void ZipArchive::StatEntry(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  auto [zip, index] = ZipArchive::CheckZipEntry(args);

  zip_stat_t file_stat;
  CHECK_EQ(zip_stat_index(zip->zip_, index, 0, &file_stat), 0);
  CHECK_LE(file_stat.comp_size, ZIP_INT64_MAX);
  CHECK_LE(file_stat.comp_size, std::numeric_limits<size_t>::max());

  zip_uint8_t opsys;
  zip_uint32_t attributes;
  CHECK_EQ(zip_file_get_external_attributes(zip->zip_, index, 0, &opsys, &attributes), 0);

  v8::Local<v8::Value> result[] = {
    v8::Integer::NewFromUnsigned(isolate, file_stat.size),
    v8::Integer::NewFromUnsigned(isolate, file_stat.comp_size),
    v8::Integer::NewFromUnsigned(isolate, file_stat.comp_method),
    v8::Integer::NewFromUnsigned(isolate, file_stat.mtime),
    v8::Integer::NewFromUnsigned(isolate, file_stat.crc),
    v8::Integer::NewFromUnsigned(isolate, opsys),
    v8::Integer::NewFromUnsigned(isolate, attributes),
  };

  args.GetReturnValue().Set(v8::Array::New(isolate, &result[0], arraysize(result)));
}

void ZipArchive::RestatEntry(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> ctx = isolate->GetCurrentContext();

  auto [zip, index] = ZipArchive::CheckZipEntry(args);

  const int argc = args.Length();
  CHECK_GE(argc, 2);

  CHECK(args[1]->IsObject());
  v8::Local<v8::Object> stats = args[1]->ToObject(ctx).ToLocalChecked();

  v8::Local<v8::Value> mtime = stats->Get(ctx, v8::String::NewFromUtf8Literal(isolate, "mtime"))
    .ToLocalChecked();
  v8::Local<v8::Value> opsys = stats->Get(ctx, v8::String::NewFromUtf8Literal(isolate, "opsys"))
    .ToLocalChecked();

  if (!mtime->IsUndefined()) {
    CHECK(IsSafeJsInt(mtime));
    time_t mtime_val = mtime.As<v8::Integer>()->Value();

    CHECK_EQ(zip_file_set_mtime(zip->zip_, index, mtime_val, 0), 0);
  }

  if (!opsys->IsUndefined()) {
    v8::Local<v8::Value> attributes = stats->Get(ctx, v8::String::NewFromUtf8Literal(isolate, "attributes"))
      .ToLocalChecked();

    CHECK(IsSafeJsInt(opsys));
    zip_uint8_t opsys_val = opsys.As<v8::Integer>()->Value();

    zip_uint32_t attributes_val = 0;
    if (!attributes->IsUndefined()) {
      CHECK(IsSafeJsInt(attributes));
      attributes_val = attributes.As<v8::Integer>()->Value();
    }

    CHECK_EQ(zip_file_set_external_attributes(zip->zip_, index, 0, opsys_val, attributes_val), 0);
  }
}

void ZipArchive::Digest(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  ZipArchive* zip = ZipArchive::CheckZip(args);

  zip_close(zip->zip_);
  zip->zip_ = nullptr;

  v8::MaybeLocal<v8::Object> buf;

  int open_res = zip_source_open(zip->source_);
  if (open_res != 0) {
    // Libzip purges the source when the file would be empty. We don't want this
    // behaviour (users can just check themselves before saving, if that's what
    // they want to do), so we check and return a predefined buffer if needed.
    zip_error_t* error = zip_source_error(zip->source_);
    CHECK_EQ(error->zip_err, ZIP_ER_DELETED);

    buf = Buffer::Copy(isolate, &emptyZipArchive[0], sizeof(emptyZipArchive));
  } else {
    CHECK_EQ(zip_source_seek(zip->source_, 0, SEEK_END), 0);
    zip_uint64_t size = zip_source_tell(zip->source_);
    CHECK_EQ(zip_source_seek(zip->source_, 0, SEEK_SET), 0);

    CHECK_LE(size, std::numeric_limits<size_t>::max());

    char* data = Malloc(size);
    CHECK_GE(zip_source_read(zip->source_, data, size), 0);

    buf = Buffer::New(isolate, data, size);

    CHECK_EQ(zip_source_close(zip->source_), 0);
  }

  args.GetReturnValue().Set(buf.ToLocalChecked());
}

int ZipArchive::GetFileType(zip_uint64_t index) const {
  zip_uint8_t opsys;
  zip_uint32_t attributes;
  CHECK_EQ(zip_file_get_external_attributes(zip_, index, 0, &opsys, &attributes), 0);

  return opsys == ZIP_OPSYS_UNIX ? (attributes >> 16) & 0xf000 : 0;
}

void NewZipArchive(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);

  CHECK(args.IsConstructCall());

  const int argc = args.Length();
  CHECK_EQ(argc, 1);

  char const* buffer_data = &emptyZipArchive[0];
  size_t buffer_length = sizeof(emptyZipArchive);

  ArrayBufferViewContents<char> buffer_view;
  if (!args[0]->IsNull()) {
    buffer_view.ReadValue(args[0]);
    buffer_data = buffer_view.data();
    buffer_length = buffer_view.length();
  }

  std::vector<char> buffer_copy(buffer_data, buffer_data + buffer_length);
  new ZipArchive(env, args.This(), std::move(buffer_copy));
}

void Initialize(v8::Local<v8::Object> target,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  v8::Isolate* isolate = env->isolate();

  v8::Local<v8::FunctionTemplate> zip = NewFunctionTemplate(isolate, NewZipArchive);
  SetProtoMethod(isolate, zip, "getEntries", ZipArchive::GetEntries);
  SetProtoMethod(isolate, zip, "addDirectory", ZipArchive::AddDirectory);
  SetProtoMethod(isolate, zip, "addEntry", ZipArchive::AddEntry);
  SetProtoMethod(isolate, zip, "deleteEntry", ZipArchive::DeleteEntry);
  SetProtoMethod(isolate, zip, "readEntry", ZipArchive::ReadEntry);
  SetProtoMethod(isolate, zip, "statEntry", ZipArchive::StatEntry);
  SetProtoMethod(isolate, zip, "restatEntry", ZipArchive::RestatEntry);
  SetProtoMethod(isolate, zip, "digest", ZipArchive::Digest);
  zip->InstanceTemplate()->SetInternalFieldCount(ZipArchive::kInternalFieldCount);
  SetConstructorFunction(env->context(), target, "ZipArchive", zip);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(NewZipArchive);
  registry->Register(ZipArchive::GetEntries);
  registry->Register(ZipArchive::AddDirectory);
  registry->Register(ZipArchive::AddEntry);
  registry->Register(ZipArchive::DeleteEntry);
  registry->Register(ZipArchive::ReadEntry);
  registry->Register(ZipArchive::StatEntry);
  registry->Register(ZipArchive::RestatEntry);
  registry->Register(ZipArchive::Digest);
}

}  // namespace archive

}  // end namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(archive, node::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(archive, node::RegisterExternalReferences)
