#include "env-inl.h"
#include "node_errors.h"
#include "node_process.h"

namespace node {
using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::NewStringType;
using v8::Nothing;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyCallbackInfo;
using v8::PropertyHandlerFlags;
using v8::String;
using v8::Value;

class RealEnvStore final : public KVStore {
 public:
  MaybeLocal<String> Get(Isolate* isolate, Local<String> key) const override;
  void Set(Isolate* isolate, Local<String> key, Local<String> value) override;
  int32_t Query(Isolate* isolate, Local<String> key) const override;
  void Delete(Isolate* isolate, Local<String> key) override;
  Local<Array> Enumerate(Isolate* isolate) const override;
};

class MapKVStore final : public KVStore {
 public:
  MaybeLocal<String> Get(Isolate* isolate, Local<String> key) const override;
  void Set(Isolate* isolate, Local<String> key, Local<String> value) override;
  int32_t Query(Isolate* isolate, Local<String> key) const override;
  void Delete(Isolate* isolate, Local<String> key) override;
  Local<Array> Enumerate(Isolate* isolate) const override;

  std::shared_ptr<KVStore> Clone(Isolate* isolate) const override;

  MapKVStore() = default;
  MapKVStore(const MapKVStore& other) : map_(other.map_) {}

 private:
  mutable Mutex mutex_;
  std::unordered_map<std::string, std::string> map_;
};

namespace per_process {
Mutex env_var_mutex;
std::shared_ptr<KVStore> system_environment = std::make_shared<RealEnvStore>();
}  // namespace per_process

MaybeLocal<String> RealEnvStore::Get(Isolate* isolate,
                                     Local<String> property) const {
  Mutex::ScopedLock lock(per_process::env_var_mutex);

  node::Utf8Value key(isolate, property);
  size_t init_sz = 256;
  MaybeStackBuffer<char, 256> val;
  int ret = uv_os_getenv(*key, *val, &init_sz);

  if (ret == UV_ENOBUFS) {
    // Buffer is not large enough, reallocate to the updated init_sz
    // and fetch env value again.
    val.AllocateSufficientStorage(init_sz);
    ret = uv_os_getenv(*key, *val, &init_sz);
  }

  if (ret >= 0) {  // Env key value fetch success.
    MaybeLocal<String> value_string =
        String::NewFromUtf8(isolate, *val, NewStringType::kNormal, init_sz);
    return value_string;
  }

  return MaybeLocal<String>();
}

void RealEnvStore::Set(Isolate* isolate,
                       Local<String> property,
                       Local<String> value) {
  Mutex::ScopedLock lock(per_process::env_var_mutex);

  node::Utf8Value key(isolate, property);
  node::Utf8Value val(isolate, value);

#ifdef _WIN32
  if (key[0] == L'=') return;
#endif
  uv_os_setenv(*key, *val);
}

int32_t RealEnvStore::Query(Isolate* isolate, Local<String> property) const {
  Mutex::ScopedLock lock(per_process::env_var_mutex);

  node::Utf8Value key(isolate, property);

  char val[2];
  size_t init_sz = sizeof(val);
  int ret = uv_os_getenv(*key, val, &init_sz);

  if (ret == UV_ENOENT) {
    return -1;
  }

#ifdef _WIN32
  if (key[0] == L'=') {
    return static_cast<int32_t>(v8::ReadOnly) |
           static_cast<int32_t>(v8::DontDelete) |
           static_cast<int32_t>(v8::DontEnum);
  }
#endif

  return 0;
}

void RealEnvStore::Delete(Isolate* isolate, Local<String> property) {
  Mutex::ScopedLock lock(per_process::env_var_mutex);

  node::Utf8Value key(isolate, property);
  uv_os_unsetenv(*key);
}

Local<Array> RealEnvStore::Enumerate(Isolate* isolate) const {
  Mutex::ScopedLock lock(per_process::env_var_mutex);
  uv_env_item_t* items;
  int count;

  OnScopeLeave cleanup([&]() { uv_os_free_environ(items, count); });
  CHECK_EQ(uv_os_environ(&items, &count), 0);

  MaybeStackBuffer<Local<Value>, 256> env_v(count);
  int env_v_index = 0;
  for (int i = 0; i < count; i++) {
#ifdef _WIN32
    // If the key starts with '=' it is a hidden environment variable.
    // The '\0' check is a workaround for the bug behind
    // https://github.com/libuv/libuv/pull/2473 and can be removed later.
    if (items[i].name[0] == '=' || items[i].name[0] == '\0') continue;
#endif
    MaybeLocal<String> str = String::NewFromUtf8(
        isolate, items[i].name, NewStringType::kNormal);
    if (str.IsEmpty()) {
      isolate->ThrowException(ERR_STRING_TOO_LONG(isolate));
      return Local<Array>();
    }
    env_v[env_v_index++] = str.ToLocalChecked();
  }

  return Array::New(isolate, env_v.out(), env_v_index);
}

std::shared_ptr<KVStore> KVStore::Clone(v8::Isolate* isolate) const {
  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();

  std::shared_ptr<KVStore> copy = KVStore::CreateMapKVStore();
  Local<Array> keys = Enumerate(isolate);
  uint32_t keys_length = keys->Length();
  for (uint32_t i = 0; i < keys_length; i++) {
    Local<Value> key = keys->Get(context, i).ToLocalChecked();
    CHECK(key->IsString());
    copy->Set(isolate,
              key.As<String>(),
              Get(isolate, key.As<String>()).ToLocalChecked());
  }
  return copy;
}

MaybeLocal<String> MapKVStore::Get(Isolate* isolate, Local<String> key) const {
  Mutex::ScopedLock lock(mutex_);
  Utf8Value str(isolate, key);
  auto it = map_.find(std::string(*str, str.length()));
  if (it == map_.end()) return Local<String>();
  return String::NewFromUtf8(isolate, it->second.data(),
                             NewStringType::kNormal, it->second.size());
}

void MapKVStore::Set(Isolate* isolate, Local<String> key, Local<String> value) {
  Mutex::ScopedLock lock(mutex_);
  Utf8Value key_str(isolate, key);
  Utf8Value value_str(isolate, value);
  if (*key_str != nullptr && *value_str != nullptr) {
    map_[std::string(*key_str, key_str.length())] =
        std::string(*value_str, value_str.length());
  }
}

int32_t MapKVStore::Query(Isolate* isolate, Local<String> key) const {
  Mutex::ScopedLock lock(mutex_);
  Utf8Value str(isolate, key);
  auto it = map_.find(std::string(*str, str.length()));
  return it == map_.end() ? -1 : 0;
}

void MapKVStore::Delete(Isolate* isolate, Local<String> key) {
  Mutex::ScopedLock lock(mutex_);
  Utf8Value str(isolate, key);
  map_.erase(std::string(*str, str.length()));
}

Local<Array> MapKVStore::Enumerate(Isolate* isolate) const {
  Mutex::ScopedLock lock(mutex_);
  std::vector<Local<Value>> values;
  values.reserve(map_.size());
  for (const auto& pair : map_) {
    values.emplace_back(
        String::NewFromUtf8(isolate, pair.first.data(),
                            NewStringType::kNormal, pair.first.size())
            .ToLocalChecked());
  }
  return Array::New(isolate, values.data(), values.size());
}

std::shared_ptr<KVStore> MapKVStore::Clone(Isolate* isolate) const {
  return std::make_shared<MapKVStore>(*this);
}

std::shared_ptr<KVStore> KVStore::CreateMapKVStore() {
  return std::make_shared<MapKVStore>();
}

Maybe<bool> KVStore::AssignFromObject(Local<Context> context,
                                      Local<Object> entries) {
  Isolate* isolate = context->GetIsolate();
  HandleScope handle_scope(isolate);
  Local<Array> keys;
  if (!entries->GetOwnPropertyNames(context).ToLocal(&keys))
    return Nothing<bool>();
  uint32_t keys_length = keys->Length();
  for (uint32_t i = 0; i < keys_length; i++) {
    Local<Value> key;
    if (!keys->Get(context, i).ToLocal(&key))
      return Nothing<bool>();
    if (!key->IsString()) continue;

    Local<Value> value;
    Local<String> value_string;
    if (!entries->Get(context, key.As<String>()).ToLocal(&value) ||
        !value->ToString(context).ToLocal(&value_string)) {
      return Nothing<bool>();
    }

    Set(isolate, key.As<String>(), value_string);
  }
  return Just(true);
}

static void EnvGetter(Local<Name> property,
                      const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  CHECK(env->has_run_bootstrapping_code());
  if (property->IsSymbol()) {
    return info.GetReturnValue().SetUndefined();
  }
  CHECK(property->IsString());
  MaybeLocal<String> value_string =
      env->env_vars()->Get(env->isolate(), property.As<String>());
  if (!value_string.IsEmpty()) {
    info.GetReturnValue().Set(value_string.ToLocalChecked());
  }
}

static void EnvSetter(Local<Name> property,
                      Local<Value> value,
                      const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  CHECK(env->has_run_bootstrapping_code());
  // calling env->EmitProcessEnvWarning() sets a variable indicating that
  // warnings have been emitted. It should be called last after other
  // conditions leading to a warning have been met.
  if (env->options()->pending_deprecation && !value->IsString() &&
      !value->IsNumber() && !value->IsBoolean() &&
      env->EmitProcessEnvWarning()) {
    if (ProcessEmitDeprecationWarning(
            env,
            "Assigning any value other than a string, number, or boolean to a "
            "process.env property is deprecated. Please make sure to convert "
            "the "
            "value to a string before setting process.env with it.",
            "DEP0104")
            .IsNothing())
      return;
  }

  Local<String> key;
  Local<String> value_string;
  if (!property->ToString(env->context()).ToLocal(&key) ||
      !value->ToString(env->context()).ToLocal(&value_string)) {
    return;
  }

  env->env_vars()->Set(env->isolate(), key, value_string);

  // Whether it worked or not, always return value.
  info.GetReturnValue().Set(value);
}

static void EnvQuery(Local<Name> property,
                     const PropertyCallbackInfo<Integer>& info) {
  Environment* env = Environment::GetCurrent(info);
  CHECK(env->has_run_bootstrapping_code());
  if (property->IsString()) {
    int32_t rc = env->env_vars()->Query(env->isolate(), property.As<String>());
    if (rc != -1) info.GetReturnValue().Set(rc);
  }
}

static void EnvDeleter(Local<Name> property,
                       const PropertyCallbackInfo<Boolean>& info) {
  Environment* env = Environment::GetCurrent(info);
  CHECK(env->has_run_bootstrapping_code());
  if (property->IsString()) {
    env->env_vars()->Delete(env->isolate(), property.As<String>());
  }

  // process.env never has non-configurable properties, so always
  // return true like the tc39 delete operator.
  info.GetReturnValue().Set(true);
}

static void EnvEnumerator(const PropertyCallbackInfo<Array>& info) {
  Environment* env = Environment::GetCurrent(info);
  CHECK(env->has_run_bootstrapping_code());

  info.GetReturnValue().Set(
      env->env_vars()->Enumerate(env->isolate()));
}

MaybeLocal<Object> CreateEnvVarProxy(Local<Context> context,
                                     Isolate* isolate,
                                     Local<Object> data) {
  EscapableHandleScope scope(isolate);
  Local<ObjectTemplate> env_proxy_template = ObjectTemplate::New(isolate);
  env_proxy_template->SetHandler(NamedPropertyHandlerConfiguration(
      EnvGetter, EnvSetter, EnvQuery, EnvDeleter, EnvEnumerator, data,
      PropertyHandlerFlags::kHasNoSideEffect));
  return scope.EscapeMaybe(env_proxy_template->NewInstance(context));
}
}  // namespace node
