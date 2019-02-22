#include "node_errors.h"
#include "node_process.h"
#include "util.h"

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#elif !defined(_MSC_VER)
extern char** environ;
#endif

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
using v8::String;
using v8::Value;

class RealEnvStore final : public KVStore {
 public:
  Local<String> Get(Isolate* isolate, Local<String> key) const override;
  void Set(Isolate* isolate, Local<String> key, Local<String> value) override;
  int32_t Query(Isolate* isolate, Local<String> key) const override;
  void Delete(Isolate* isolate, Local<String> key) override;
  Local<Array> Enumerate(Isolate* isolate) const override;
};

class MapKVStore final : public KVStore {
 public:
  Local<String> Get(Isolate* isolate, Local<String> key) const override;
  void Set(Isolate* isolate, Local<String> key, Local<String> value) override;
  int32_t Query(Isolate* isolate, Local<String> key) const override;
  void Delete(Isolate* isolate, Local<String> key) override;
  Local<Array> Enumerate(Isolate* isolate) const override;

  std::shared_ptr<KVStore> Clone(Isolate* isolate) const override;

  MapKVStore() {}
  MapKVStore(const MapKVStore& other) : map_(other.map_) {}

 private:
  mutable Mutex mutex_;
  std::unordered_map<std::string, std::string> map_;
};

namespace per_process {
Mutex env_var_mutex;
std::shared_ptr<KVStore> system_environment = std::make_shared<RealEnvStore>();
}  // namespace per_process

Local<String> RealEnvStore::Get(Isolate* isolate,
                                Local<String> property) const {
  Mutex::ScopedLock lock(per_process::env_var_mutex);
#ifdef __POSIX__
  node::Utf8Value key(isolate, property);
  const char* val = getenv(*key);
  if (val) {
    return String::NewFromUtf8(isolate, val, NewStringType::kNormal)
        .ToLocalChecked();
  }
#else  // _WIN32
  node::TwoByteValue key(isolate, property);
  WCHAR buffer[32767];  // The maximum size allowed for environment variables.
  SetLastError(ERROR_SUCCESS);
  DWORD result = GetEnvironmentVariableW(
      reinterpret_cast<WCHAR*>(*key), buffer, arraysize(buffer));
  // If result >= sizeof buffer the buffer was too small. That should never
  // happen. If result == 0 and result != ERROR_SUCCESS the variable was not
  // found.
  if ((result > 0 || GetLastError() == ERROR_SUCCESS) &&
      result < arraysize(buffer)) {
    const uint16_t* two_byte_buffer = reinterpret_cast<const uint16_t*>(buffer);
    v8::MaybeLocal<String> rc = String::NewFromTwoByte(
        isolate, two_byte_buffer, NewStringType::kNormal);
    if (rc.IsEmpty()) {
      isolate->ThrowException(ERR_STRING_TOO_LONG(isolate));
      return Local<String>();
    }
    return rc.ToLocalChecked();
  }
#endif
  return Local<String>();
}

void RealEnvStore::Set(Isolate* isolate,
                       Local<String> property,
                       Local<String> value) {
  Mutex::ScopedLock lock(per_process::env_var_mutex);
#ifdef __POSIX__
  node::Utf8Value key(isolate, property);
  node::Utf8Value val(isolate, value);
  setenv(*key, *val, 1);
#else  // _WIN32
  node::TwoByteValue key(isolate, property);
  node::TwoByteValue val(isolate, value);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  // Environment variables that start with '=' are read-only.
  if (key_ptr[0] != L'=') {
    SetEnvironmentVariableW(key_ptr, reinterpret_cast<WCHAR*>(*val));
  }
#endif
}

int32_t RealEnvStore::Query(Isolate* isolate, Local<String> property) const {
  Mutex::ScopedLock lock(per_process::env_var_mutex);
#ifdef __POSIX__
  node::Utf8Value key(isolate, property);
  if (getenv(*key)) return 0;
#else  // _WIN32
  node::TwoByteValue key(isolate, property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  SetLastError(ERROR_SUCCESS);
  if (GetEnvironmentVariableW(key_ptr, nullptr, 0) > 0 ||
      GetLastError() == ERROR_SUCCESS) {
    if (key_ptr[0] == L'=') {
      // Environment variables that start with '=' are hidden and read-only.
      return static_cast<int32_t>(v8::ReadOnly) |
             static_cast<int32_t>(v8::DontDelete) |
             static_cast<int32_t>(v8::DontEnum);
    }
    return 0;
  }
#endif
  return -1;
}

void RealEnvStore::Delete(Isolate* isolate, Local<String> property) {
  Mutex::ScopedLock lock(per_process::env_var_mutex);
#ifdef __POSIX__
  node::Utf8Value key(isolate, property);
  unsetenv(*key);
#else
  node::TwoByteValue key(isolate, property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  SetEnvironmentVariableW(key_ptr, nullptr);
#endif
}

Local<Array> RealEnvStore::Enumerate(Isolate* isolate) const {
  Mutex::ScopedLock lock(per_process::env_var_mutex);
#ifdef __POSIX__
  int env_size = 0;
  while (environ[env_size]) {
    env_size++;
  }
  std::vector<Local<Value>> env_v(env_size);

  for (int i = 0; i < env_size; ++i) {
    const char* var = environ[i];
    const char* s = strchr(var, '=');
    const int length = s ? s - var : strlen(var);
    env_v[i] = String::NewFromUtf8(isolate, var, NewStringType::kNormal, length)
                   .ToLocalChecked();
  }
#else  // _WIN32
  std::vector<Local<Value>> env_v;
  WCHAR* environment = GetEnvironmentStringsW();
  if (environment == nullptr)
    return Array::New(isolate);  // This should not happen.
  WCHAR* p = environment;
  while (*p) {
    WCHAR* s;
    if (*p == L'=') {
      // If the key starts with '=' it is a hidden environment variable.
      p += wcslen(p) + 1;
      continue;
    } else {
      s = wcschr(p, L'=');
    }
    if (!s) {
      s = p + wcslen(p);
    }
    const uint16_t* two_byte_buffer = reinterpret_cast<const uint16_t*>(p);
    const size_t two_byte_buffer_len = s - p;
    v8::MaybeLocal<String> rc = String::NewFromTwoByte(
        isolate, two_byte_buffer, NewStringType::kNormal, two_byte_buffer_len);
    if (rc.IsEmpty()) {
      isolate->ThrowException(ERR_STRING_TOO_LONG(isolate));
      FreeEnvironmentStringsW(environment);
      return Local<Array>();
    }
    env_v.push_back(rc.ToLocalChecked());
    p = s + wcslen(s) + 1;
  }
  FreeEnvironmentStringsW(environment);
#endif

  return Array::New(isolate, env_v.data(), env_v.size());
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
    copy->Set(isolate, key.As<String>(), Get(isolate, key.As<String>()));
  }
  return copy;
}

Local<String> MapKVStore::Get(Isolate* isolate, Local<String> key) const {
  Mutex::ScopedLock lock(mutex_);
  Utf8Value str(isolate, key);
  auto it = map_.find(std::string(*str, str.length()));
  if (it == map_.end()) return Local<String>();
  return String::NewFromUtf8(isolate, it->second.data(),
                             NewStringType::kNormal, it->second.size())
      .ToLocalChecked();
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
  if (property->IsSymbol()) {
    return info.GetReturnValue().SetUndefined();
  }
  CHECK(property->IsString());
  info.GetReturnValue().Set(
      env->env_vars()->Get(env->isolate(), property.As<String>()));
}

static void EnvSetter(Local<Name> property,
                      Local<Value> value,
                      const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
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
  if (property->IsString()) {
    int32_t rc = env->env_vars()->Query(env->isolate(), property.As<String>());
    if (rc != -1) info.GetReturnValue().Set(rc);
  }
}

static void EnvDeleter(Local<Name> property,
                       const PropertyCallbackInfo<Boolean>& info) {
  Environment* env = Environment::GetCurrent(info);
  if (property->IsString()) {
    env->env_vars()->Delete(env->isolate(), property.As<String>());
  }

  // process.env never has non-configurable properties, so always
  // return true like the tc39 delete operator.
  info.GetReturnValue().Set(true);
}

static void EnvEnumerator(const PropertyCallbackInfo<Array>& info) {
  Environment* env = Environment::GetCurrent(info);

  info.GetReturnValue().Set(
      env->env_vars()->Enumerate(env->isolate()));
}

MaybeLocal<Object> CreateEnvVarProxy(Local<Context> context,
                                     Isolate* isolate,
                                     Local<Object> data) {
  EscapableHandleScope scope(isolate);
  Local<ObjectTemplate> env_proxy_template = ObjectTemplate::New(isolate);
  env_proxy_template->SetHandler(NamedPropertyHandlerConfiguration(
      EnvGetter, EnvSetter, EnvQuery, EnvDeleter, EnvEnumerator, data));
  return scope.EscapeMaybe(env_proxy_template->NewInstance(context));
}
}  // namespace node
