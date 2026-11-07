#include "node_blob.h"
#include "ada.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_bob-inl.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_file.h"
#include "path.h"
#include "permission/permission.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

#include <algorithm>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BackingStoreInitializationMode;
using v8::BackingStoreOnFailureMode;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::SnapshotCreator;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;

namespace {

// Concatenate multiple ArrayBufferView/ArrayBuffers into a single ArrayBuffer.
// This method treats all ArrayBufferView types the same.
void Concat(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  CHECK(args[0]->IsArray());
  Local<Array> array = args[0].As<Array>();

  struct View {
    std::shared_ptr<BackingStore> store;
    size_t length;
    size_t offset = 0;
  };

  std::vector<View> views;
  size_t total = 0;

  std::vector<Global<Value>> buffers;
  if (FromV8Array(context, array, &buffers).IsNothing()) {
    return;
  }

  size_t count = buffers.size();
  for (uint32_t i = 0; i < count; i++) {
    Local<Value> val = buffers[i].Get(isolate);
    if (val->IsArrayBuffer()) {
      auto ab = val.As<ArrayBuffer>();
      views.push_back(View{ab->GetBackingStore(), ab->ByteLength(), 0});
      total += ab->ByteLength();
    } else {
      CHECK(val->IsArrayBufferView());
      auto view = val.As<ArrayBufferView>();
      views.push_back(View{view->Buffer()->GetBackingStore(),
                           view->ByteLength(),
                           view->ByteOffset()});
      total += view->ByteLength();
    }
  }

  std::shared_ptr<BackingStore> store = ArrayBuffer::NewBackingStore(
      isolate,
      total,
      BackingStoreInitializationMode::kUninitialized,
      BackingStoreOnFailureMode::kReturnNull);
  if (!store) [[unlikely]] {
    THROW_ERR_MEMORY_ALLOCATION_FAILED(isolate);
    return;
  }
  uint8_t* ptr = static_cast<uint8_t*>(store->Data());
  for (size_t n = 0; n < views.size(); n++) {
    uint8_t* from =
        static_cast<uint8_t*>(views[n].store->Data()) + views[n].offset;
    std::copy(from, from + views[n].length, ptr);
    ptr += views[n].length;
  }

  args.GetReturnValue().Set(ArrayBuffer::New(isolate, std::move(store)));
}

// uv_cwd() fails closed: a relative path is left unchanged rather than pinned
// to exec_path, which would select a different file.
std::string GetProcessCwd() {
  size_t size = PATH_MAX_BYTES;
  std::string buf(size, '\0');
  int err = uv_cwd(buf.data(), &size);
  if (err == UV_ENOBUFS) {
    buf.assign(size, '\0');
    err = uv_cwd(buf.data(), &size);
  }
  if (err != 0) return {};
  buf.resize(strlen(buf.data()));
  return buf;
}

#ifdef _WIN32
std::string WidePathToWtf8(const wchar_t* wide, size_t chars) {
  const uint16_t* src = reinterpret_cast<const uint16_t*>(wide);
  const ssize_t src_len = static_cast<ssize_t>(chars);
  const size_t bytes = uv_utf16_length_as_wtf8(src, src_len);
  // uv_utf16_to_wtf8 writes a trailing NUL past the byte length.
  std::string out(bytes + 1, '\0');
  char* ptr = out.data();
  size_t len = bytes;
  if (uv_utf16_to_wtf8(src, src_len, &ptr, &len) != 0) return {};
  out.resize(len);
  return out;
}

std::string DriveRoot(char letter) {
  std::string root;
  root.push_back(letter);
  root.append(":\\");
  return root;
}

// Current directory of one drive, without the caller's relative suffix.
// GetFullPathNameW("C:") is the directory CreateFileW uses for "C:foo".
std::string DirectoryOnDrive(char letter) {
  const wchar_t spec[3] = {static_cast<wchar_t>(letter), L':', L'\0'};
  constexpr DWORD kPathChars = 32768;
  std::wstring wide(kPathChars, L'\0');
  const DWORD written =
      GetFullPathNameW(spec, kPathChars, wide.data(), nullptr);
  if (written == 0 || written >= kPathChars) return DriveRoot(letter);
  std::string converted = WidePathToWtf8(wide.data(), written);
  if (converted.empty()) return DriveRoot(letter);
  return converted;
}

// "C:" or "\\server\share" (or the \\?\ equivalent) from an absolute path.
std::string VolumePrefix(std::string_view absolute) {
  if (absolute.size() >= 2 && IsWindowsDeviceRoot(absolute[0]) &&
      absolute[1] == ':') {
    return std::string(absolute.substr(0, 2));
  }
  if (absolute.size() < 2 || !IsPathSeparator(absolute[0]) ||
      !IsPathSeparator(absolute[1])) {
    return {};
  }

  size_t component_end = 2;
  while (component_end < absolute.size() &&
         !IsPathSeparator(absolute[component_end])) {
    component_end++;
  }
  if (component_end >= absolute.size()) return std::string(absolute);
  const std::string_view first = absolute.substr(2, component_end - 2);
  size_t prefix_end = component_end;
  if (first == "?" || first == ".") {
    const size_t after = component_end + 1;
    const bool unc =
        after + 3 <= absolute.size() &&
        (absolute.substr(after, 3) == "UNC" ||
         absolute.substr(after, 3) == "unc") &&
        (after + 3 == absolute.size() || IsPathSeparator(absolute[after + 3]));
    size_t cursor = unc ? after + 4 : after;
    const int parts = unc ? 2 : 1;
    for (int part = 0; part < parts; part++) {
      while (cursor < absolute.size() && !IsPathSeparator(absolute[cursor])) {
        cursor++;
      }
      prefix_end = cursor;
      if (part + 1 < parts) cursor++;
    }
  } else {
    size_t cursor = component_end + 1;
    while (cursor < absolute.size() && !IsPathSeparator(absolute[cursor])) {
      cursor++;
    }
    prefix_end = cursor;
  }
  return std::string(absolute.substr(0, prefix_end));
}

std::string JoinAbsolute(std::string_view prefix, std::string_view rest) {
  std::string out(prefix);
  if (rest.empty()) return out;
  const bool prefix_sep = !prefix.empty() && IsPathSeparator(prefix.back());
  const bool rest_sep = IsPathSeparator(rest.front());
  if (!prefix_sep && !rest_sep) out.push_back('\\');
  if (prefix_sep && rest_sep) rest.remove_prefix(1);
  out.append(rest);
  return out;
}

bool HasAbsoluteBlobRoot(std::string_view path) {
  if (path.size() >= 2 && IsPathSeparator(path[0]) &&
      IsPathSeparator(path[1])) {
    return true;
  }
  return path.size() >= 3 && IsWindowsDeviceRoot(path[0]) && path[1] == ':' &&
         IsPathSeparator(path[2]);
}
#endif  // _WIN32

// Make a path independent of later process.chdir() calls without removing
// "." or "..". Lexical normalization would resolve ".." before symlinks and
// could open a different file than the path the caller passed.
std::string PinBlobBackingPath(std::string_view path) {
  if (path.empty()) return std::string(path);
#ifdef _WIN32
  if (HasAbsoluteBlobRoot(path)) return std::string(path);
  if (path.size() >= 2 && IsWindowsDeviceRoot(path[0]) && path[1] == ':') {
    return JoinAbsolute(DirectoryOnDrive(path[0]), path.substr(2));
  }
  if (IsPathSeparator(path[0])) {
    const std::string volume = VolumePrefix(GetProcessCwd());
    if (volume.empty()) return std::string(path);
    return JoinAbsolute(volume, path);
  }
  const char separator = '\\';
#else
  if (path[0] == '/') return std::string(path);
  const char separator = '/';
#endif
  const std::string cwd = GetProcessCwd();
  if (cwd.empty()) return std::string(path);
  std::string pinned;
  pinned.reserve(cwd.size() + path.size() + 1);
  pinned.append(cwd);
  if (!IsPathSeparator(cwd.back())) pinned.push_back(separator);
  pinned.append(path);
  return pinned;
}

void BlobFromFilePath(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  BufferValue path(env->isolate(), args[0]);
  CHECK_NOT_NULL(*path);
  // Snapshot the caller bytes before ToNamespacedPath() rewrites `path` on
  // Windows. Permission checks and stat errors keep using that rewritten
  // buffer, so their diagnostics stay the same.
  const std::string original(path.out(), path.length());
  ToNamespacedPath(env, &path);
  THROW_IF_INSUFFICIENT_PERMISSIONS(
      env, permission::PermissionScope::kFileSystemRead, path.ToStringView());

  const std::string pinned = PinBlobBackingPath(original);
  Local<Value> stored_path = args[0];
  if (pinned != original) {
    // A V8 string would UTF-8-decode a POSIX Buffer path. Copy the raw bytes.
    if (!Buffer::Copy(env->isolate(), pinned.data(), pinned.size())
             .ToLocal(&stored_path)) {
      return;
    }
  }

  int status = 0;
  auto entry = DataQueue::CreateFdEntry(env, stored_path, &status);
  if (entry == nullptr) {
    // The file could not be stat'd. Report the libuv error so callers can tell
    // ENOENT apart from any other reason the path could not be used.
    return env->ThrowUVException(status, "stat", nullptr, *path);
  }

  std::vector<std::unique_ptr<DataQueue::Entry>> entries;
  entries.push_back(std::move(entry));

  if (auto blob =
          Blob::Create(env, DataQueue::CreateIdempotent(std::move(entries)))) {
    Local<Value> vals[2]{
        blob->object(),
        Number::New(env->isolate(), static_cast<double>(blob->length())),
    };
    args.GetReturnValue().Set(
        Array::New(env->isolate(), &vals[0], arraysize(vals)));
  }
}
}  // namespace

void Blob::CreatePerIsolateProperties(IsolateData* isolate_data,
                                      Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();

  SetMethod(isolate, target, "createBlob", New);
  SetMethod(isolate, target, "storeDataObject", StoreDataObject);
  SetMethod(isolate, target, "getDataObject", GetDataObject);
  SetMethod(isolate, target, "revokeObjectURL", RevokeObjectURL);
  SetMethod(isolate, target, "concat", Concat);
  SetMethod(isolate, target, "createBlobFromFilePath", BlobFromFilePath);
}

void Blob::CreatePerContextProperties(Local<Object> target,
                                      Local<Value> unused,
                                      Local<Context> context,
                                      void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  realm->AddBindingData<BlobBindingData>(target);
}

Local<FunctionTemplate> Blob::GetConstructorTemplate(Environment* env) {
  Local<FunctionTemplate> tmpl = env->blob_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(Blob::kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Blob"));
    SetProtoMethod(isolate, tmpl, "getReader", GetReader);
    SetProtoMethod(isolate, tmpl, "slice", ToSlice);
    env->set_blob_constructor_template(tmpl);
  }
  return tmpl;
}

bool Blob::HasInstance(Environment* env, Local<Value> object) {
  return GetConstructorTemplate(env)->HasInstance(object);
}

BaseObjectPtr<Blob> Blob::Create(Environment* env,
                                 std::shared_ptr<DataQueue> data_queue) {
  HandleScope scope(env->isolate());

  Local<Function> ctor;
  if (!GetConstructorTemplate(env)->GetFunction(env->context()).ToLocal(&ctor))
    return nullptr;

  Local<Object> obj;
  if (!ctor->NewInstance(env->context()).ToLocal(&obj)) return nullptr;

  return MakeBaseObject<Blob>(env, obj, data_queue);
}

void Blob::New(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Environment* env = Environment::GetCurrent(context);

  CHECK(args[0]->IsArray());  // sources

  Local<Array> array = args[0].As<Array>();
  std::vector<std::unique_ptr<DataQueue::Entry>> entries(array->Length());

  std::vector<Global<Value>> sources;
  if (FromV8Array(context, array, &sources).IsNothing()) {
    return;
  }

  size_t count = sources.size();
  for (size_t i = 0; i < count; i++) {
    Local<Value> entry = sources[i].Get(isolate);

    auto entryFromArrayBuffer =
        [isolate](Local<ArrayBuffer> buf,
                  size_t byte_length,
                  size_t byte_offset =
                      0) mutable -> std::unique_ptr<DataQueue::Entry> {
      if (buf->IsDetachable()) {
        std::shared_ptr<BackingStore> store = buf->GetBackingStore();
        if (buf->Detach(Local<Value>()).IsNothing()) {
          return nullptr;
        }
        return DataQueue::CreateInMemoryEntryFromBackingStore(
            std::move(store), byte_offset, byte_length);
      }

      // If the ArrayBuffer is not detachable, we will copy from it instead.
      std::shared_ptr<BackingStore> store = ArrayBuffer::NewBackingStore(
          isolate, byte_length, BackingStoreInitializationMode::kUninitialized);
      uint8_t* ptr = static_cast<uint8_t*>(buf->Data()) + byte_offset;
      std::copy(ptr, ptr + byte_length, static_cast<uint8_t*>(store->Data()));
      return DataQueue::CreateInMemoryEntryFromBackingStore(
          std::move(store), 0, byte_length);
    };

    // Every entry should be either an ArrayBuffer, ArrayBufferView, or Blob.
    // If the input to the Blob constructor in JavaScript was a string, then
    // it will be decoded into an ArrayBufferView there before being passed
    // in.
    //
    // Importantly, here we also assume that the ArrayBuffer/ArrayBufferView
    // is not going to be modified here so we will detach them. It is up to
    // the JavaScript side to do the right thing with regards to copying and
    // ensuring appropriate spec compliance.
    if (entry->IsArrayBuffer()) {
      Local<ArrayBuffer> buf = entry.As<ArrayBuffer>();
      auto ret = entryFromArrayBuffer(buf, buf->ByteLength());
      if (!ret) return;
      entries[i] = std::move(ret);
    } else if (entry->IsArrayBufferView()) {
      Local<ArrayBufferView> view = entry.As<ArrayBufferView>();
      auto ret = entryFromArrayBuffer(
          view->Buffer(), view->ByteLength(), view->ByteOffset());
      if (!ret) return;
      entries[i] = std::move(ret);
    } else if (Blob::HasInstance(env, entry)) {
      Blob* blob;
      ASSIGN_OR_RETURN_UNWRAP(&blob, entry);
      entries[i] = DataQueue::CreateDataQueueEntry(blob->data_queue_);
    } else {
      UNREACHABLE("Incorrect Blob initialization type");
    }
  }

  auto blob = Create(env, DataQueue::CreateIdempotent(std::move(entries)));
  if (blob) args.GetReturnValue().Set(blob->object());
}

void Blob::GetReader(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Blob* blob;
  ASSIGN_OR_RETURN_UNWRAP(&blob, args.This());

  BaseObjectPtr<Blob::Reader> reader =
      Blob::Reader::Create(env, BaseObjectPtr<Blob>(blob));
  if (reader) args.GetReturnValue().Set(reader->object());
}

void Blob::ToSlice(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Blob* blob;
  ASSIGN_OR_RETURN_UNWRAP(&blob, args.This());
  CHECK(args[0]->IsNumber());
  CHECK(args[1]->IsNumber());
  uint64_t start = static_cast<uint64_t>(args[0].As<Number>()->Value());
  uint64_t end = static_cast<uint64_t>(args[1].As<Number>()->Value());
  BaseObjectPtr<Blob> slice = blob->Slice(env, start, end);
  if (slice) args.GetReturnValue().Set(slice->object());
}

void Blob::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("data_queue_", data_queue_, "std::shared_ptr<DataQueue>");
}

BaseObjectPtr<Blob> Blob::Slice(Environment* env,
                                uint64_t start,
                                uint64_t end) {
  return Create(env, this->data_queue_->slice(start, end));
}

Blob::Blob(Environment* env,
           Local<Object> obj,
           std::shared_ptr<DataQueue> data_queue)
    : BaseObject(env, obj), data_queue_(data_queue) {
  MakeWeak();
}

Blob::Reader::Reader(Environment* env,
                     Local<Object> obj,
                     BaseObjectPtr<Blob> strong_ptr)
    : AsyncWrap(env, obj, AsyncWrap::PROVIDER_BLOBREADER),
      inner_(strong_ptr->data_queue_->get_reader()),
      strong_ptr_(std::move(strong_ptr)) {
  MakeWeak();
}

bool Blob::Reader::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> Blob::Reader::GetConstructorTemplate(Environment* env) {
  Local<FunctionTemplate> tmpl = env->blob_reader_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Blob::Reader::kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "BlobReader"));
    SetProtoMethod(env->isolate(), tmpl, "pull", Pull);
    SetProtoMethod(env->isolate(), tmpl, "setWakeup", SetWakeup);
    env->set_blob_reader_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<Blob::Reader> Blob::Reader::Create(Environment* env,
                                                 BaseObjectPtr<Blob> blob) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<Blob::Reader>(env, obj, std::move(blob));
}

void Blob::Reader::Pull(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Blob::Reader* reader;
  ASSIGN_OR_RETURN_UNWRAP(&reader, args.This());
  reader->pull_pending_ = false;

  CHECK(args[0]->IsFunction());
  Local<Function> fn = args[0].As<Function>();
  CHECK(!fn->IsConstructor());

  if (reader->eos_) {
    Local<Value> arg = Int32::New(env->isolate(), bob::STATUS_EOS);
    reader->MakeCallback(fn, 1, &arg);
    return args.GetReturnValue().Set(bob::STATUS_EOS);
  }

  struct Impl {
    BaseObjectPtr<Blob::Reader> reader;
    Global<Function> callback;
    Environment* env;
  };
  // TODO(@jasnell): A unique_ptr is likely better here but making this a unique
  // pointer that is passed into the lambda causes the std::move(next) below to
  // complain about std::function needing to be copy-constructible.
  Impl* impl = new Impl();
  impl->reader = BaseObjectPtr<Blob::Reader>(reader);
  impl->callback.Reset(env->isolate(), fn);
  impl->env = env;

  auto next = [impl](int status,
                     const DataQueue::Vec* vecs,
                     size_t count,
                     bob::Done doneCb) mutable {
    auto dropMe = std::unique_ptr<Impl>(impl);
    Environment* env = impl->env;
    HandleScope handleScope(env->isolate());
    Local<Function> fn = impl->callback.Get(env->isolate());

    if (status == bob::STATUS_EOS) impl->reader->eos_ = true;

    if (count > 0) {
      // Copy the returns vectors into a single ArrayBuffer.
      size_t total = 0;
      for (size_t n = 0; n < count; n++) total += vecs[n].len;

      std::shared_ptr<BackingStore> store = ArrayBuffer::NewBackingStore(
          env->isolate(),
          total,
          BackingStoreInitializationMode::kUninitialized);
      auto ptr = static_cast<uint8_t*>(store->Data());
      for (size_t n = 0; n < count; n++) {
        std::copy(vecs[n].base, vecs[n].base + vecs[n].len, ptr);
        ptr += vecs[n].len;
      }
      // Since we copied the data buffers, signal that we're done with them.
      std::move(doneCb)(0);
      Local<Value> argv[2] = {Uint32::New(env->isolate(), status),
                              ArrayBuffer::New(env->isolate(), store)};
      impl->reader->MakeCallback(fn, arraysize(argv), argv);
      return;
    }

    Local<Value> argv[2] = {
        Int32::New(env->isolate(), status),
        Undefined(env->isolate()),
    };
    impl->reader->MakeCallback(fn, arraysize(argv), argv);
  };

  args.GetReturnValue().Set(reader->inner_->Pull(
      std::move(next), node::bob::OPTIONS_END, nullptr, 0));
}

void Blob::Reader::SetWakeup(const FunctionCallbackInfo<Value>& args) {
  Blob::Reader* reader;
  ASSIGN_OR_RETURN_UNWRAP(&reader, args.This());
  if (args[0]->IsUndefined()) {
    reader->wakeup_.Reset();
    return;
  }
  CHECK(args[0]->IsFunction());
  reader->wakeup_.Reset(args.GetIsolate(), args[0].As<Function>());
}

void Blob::Reader::NotifyPull() {
  if (wakeup_.IsEmpty() || !env()->can_call_into_js()) return;
  // Coalesce notifications: if a wakeup is already pending and the reader
  // has not yet pulled, skip re-notifying to avoid redundant wakeups
  // within a single packet.
  if (pull_pending_) return;
  pull_pending_ = true;
  HandleScope handle_scope(env()->isolate());
  Local<Function> fn = wakeup_.Get(env()->isolate());
  MakeCallback(fn, 0, nullptr);
}

BaseObjectPtr<BaseObject> Blob::BlobTransferData::Deserialize(
    Environment* env,
    Local<Context> context,
    std::unique_ptr<worker::TransferData> self) {
  if (context != env->context()) {
    THROW_ERR_MESSAGE_TARGET_CONTEXT_UNAVAILABLE(env);
    return {};
  }
  return Blob::Create(env, data_queue);
}

BaseObject::TransferMode Blob::GetTransferMode() const {
  return TransferMode::kCloneable;
}

std::unique_ptr<worker::TransferData> Blob::CloneForMessaging() const {
  return std::make_unique<BlobTransferData>(data_queue_);
}

void Blob::StoreDataObject(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);

  CHECK(args[0]->IsString());                       // ID key
  CHECK(Blob::HasInstance(realm->env(), args[1]));  // Blob
  CHECK(args[2]->IsNumber());                       // Length
  CHECK(args[3]->IsString());                       // Type

  BlobBindingData* binding_data = realm->GetBindingData<BlobBindingData>();
  Isolate* isolate = realm->isolate();

  Utf8Value key(isolate, args[0]);
  Blob* blob;
  ASSIGN_OR_RETURN_UNWRAP(&blob, args[1]);

  uint64_t length = static_cast<uint64_t>(args[2].As<Number>()->Value());
  Utf8Value type(isolate, args[3]);

  binding_data->store_data_object(
      key.ToString(),
      BlobBindingData::StoredDataObject(
          BaseObjectPtr<Blob>(blob), length, type.ToString()));
}

// Note: applying the V8 Fast API to the following function does not produce
//       performance benefits (ref: https://github.com/nodejs/node/pull/58544)
void Blob::RevokeObjectURL(const FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());
  Realm* realm = Realm::GetCurrent(args);
  BlobBindingData* binding_data = realm->GetBindingData<BlobBindingData>();
  Isolate* isolate = realm->isolate();

  Utf8Value input(isolate, args[0].As<String>());
  auto out = ada::parse<ada::url_aggregator>(input.ToStringView());

  if (!out) {
    return;
  }

  auto pathname = out->get_pathname();
  auto start_index = pathname.find(':');

  if (start_index != std::string_view::npos && start_index != pathname.size()) {
    auto end_index = pathname.find(':', start_index + 1);
    if (end_index == std::string_view::npos) {
      auto id = std::string(pathname.substr(start_index + 1));
      binding_data->revoke_data_object(id);
    }
  }
}

void Blob::GetDataObject(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  Realm* realm = Realm::GetCurrent(args);
  BlobBindingData* binding_data = realm->GetBindingData<BlobBindingData>();
  Isolate* isolate = realm->isolate();

  Utf8Value key(isolate, args[0]);

  BlobBindingData::StoredDataObject stored =
      binding_data->get_data_object(key.ToString());
  if (stored.blob) {
    Local<Value> type;
    if (!String::NewFromUtf8(isolate,
                             stored.type.c_str(),
                             NewStringType::kNormal,
                             static_cast<int>(stored.type.length()))
             .ToLocal(&type)) {
      return;
    }

    Local<Value> values[] = {
        stored.blob->object(),
        Number::New(isolate, static_cast<double>(stored.length)),
        type};

    args.GetReturnValue().Set(Array::New(isolate, values, arraysize(values)));
  }
}

void BlobBindingData::StoredDataObject::MemoryInfo(
    MemoryTracker* tracker) const {
  tracker->TrackField("blob", blob, "BaseObjectPtr<Blob>");
}

BlobBindingData::StoredDataObject::StoredDataObject(
    const BaseObjectPtr<Blob>& blob_,
    uint64_t length_,
    const std::string& type_)
    : blob(blob_), length(length_), type(type_) {}

BlobBindingData::BlobBindingData(Realm* realm, Local<Object> wrap)
    : SnapshotableObject(realm, wrap, type_int) {
  MakeWeak();
}

void BlobBindingData::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("data_objects_",
                      data_objects_,
                      "std::unordered_map<std::string, StoredDataObject>");
}

void BlobBindingData::store_data_object(
    const std::string& uuid, const BlobBindingData::StoredDataObject& object) {
  data_objects_[uuid] = object;
}

void BlobBindingData::revoke_data_object(const std::string& uuid) {
  if (!data_objects_.contains(uuid)) {
    return;
  }
  data_objects_.erase(uuid);
  CHECK(!data_objects_.contains(uuid));
}

BlobBindingData::StoredDataObject BlobBindingData::get_data_object(
    const std::string& uuid) {
  auto entry = data_objects_.find(uuid);
  if (entry == data_objects_.end()) return BlobBindingData::StoredDataObject{};
  return entry->second;
}

void BlobBindingData::Deserialize(Local<Context> context,
                                  Local<Object> holder,
                                  int index,
                                  InternalFieldInfoBase* info) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  HandleScope scope(Isolate::GetCurrent());
  Realm* realm = Realm::GetCurrent(context);
  BlobBindingData* binding = realm->AddBindingData<BlobBindingData>(holder);
  CHECK_NOT_NULL(binding);
}

bool BlobBindingData::PrepareForSerialization(Local<Context> context,
                                              SnapshotCreator* creator) {
  // Stored blob objects are not actually persisted.
  // Return true because we need to maintain the reference to the binding from
  // JS land.
  return true;
}

InternalFieldInfoBase* BlobBindingData::Serialize(int index) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  InternalFieldInfo* info =
      InternalFieldInfoBase::New<InternalFieldInfo>(type());
  return info;
}

void Blob::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(Blob::New);
  registry->Register(Blob::GetReader);
  registry->Register(Blob::ToSlice);
  registry->Register(Blob::StoreDataObject);
  registry->Register(Blob::GetDataObject);
  registry->Register(Blob::RevokeObjectURL);
  registry->Register(Blob::Reader::Pull);
  registry->Register(Blob::Reader::SetWakeup);
  registry->Register(Concat);
  registry->Register(BlobFromFilePath);
}

}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(blob,
                                    node::Blob::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(blob, node::Blob::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(blob, node::Blob::RegisterExternalReferences)
