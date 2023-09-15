#include "env-inl.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "memory_tracker-inl.h"
#include "node_mem-inl.h"
#include "util-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_webstorage.h"
#include "sqlite3.h"

namespace node {
namespace webstorage {

using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::DontDelete;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::IndexedPropertyHandlerConfiguration;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Map;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::Null;
using v8::Object;
using v8::PropertyCallbackInfo;
using v8::PropertyDescriptor;
using v8::PropertyHandlerFlags;
using v8::String;
using v8::Uint32;
using v8::Value;

static constexpr const int kMaxStorageSize = 10 * 1024 * 1024;  // 10 MB

#define THROW_SQLITE_ERROR(env, r)                                            \
  node::THROW_ERR_INVALID_STATE((env), sqlite3_errstr((r)))

#define CHECK_ERROR_OR_THROW(env, expr, expected, ret)                        \
  do {                                                                        \
    int _r = (expr);                                                          \
    if (_r != (expected)) {                                                   \
      THROW_SQLITE_ERROR((env), _r);                                          \
      return (ret);                                                           \
    }                                                                         \
  } while (0)

static void ThrowQuotaExceededException(Local<Context> context) {
  Isolate* isolate = context->GetIsolate();
  auto dom_exception_str = FIXED_ONE_BYTE_STRING(isolate, "DOMException");
  auto err_name = FIXED_ONE_BYTE_STRING(isolate, "QuotaExceededError");
  auto err_message = FIXED_ONE_BYTE_STRING(isolate,
      "Setting the value exceeded the quota");
  Local<Object> per_context_bindings;
  Local<Value> domexception_ctor_val;
  if (!GetPerContextExports(context).ToLocal(&per_context_bindings) ||
      !per_context_bindings->Get(context, dom_exception_str)
          .ToLocal(&domexception_ctor_val)) {
    return;
  }
  CHECK(domexception_ctor_val->IsFunction());
  Local<Function> domexception_ctor = domexception_ctor_val.As<Function>();
  Local<Value> argv[] = {err_message, err_name};
  Local<Value> exception;

  if (!domexception_ctor->NewInstance(context, arraysize(argv), argv)
           .ToLocal(&exception)) {
    return;
  }

  isolate->ThrowException(exception);
}

Storage::Storage(
    Environment* env, Local<Object> object, Local<String> location) :
    BaseObject(env, object) {
  MakeWeak();
  node::Utf8Value utf8_location(env->isolate(), location);
  _symbols.Reset(env->isolate(), Map::New(env->isolate()));
  _db = nullptr;
  _location = utf8_location.ToString();
}

Storage::~Storage() {
  sqlite3_close(_db);
  _db = nullptr;
}

void Storage::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("symbols", _symbols);
}

bool Storage::Open() {
  static const char sql[] = "CREATE TABLE IF NOT EXISTS nodejs_webstorage("
                            "  key BLOB NOT NULL,"
                            "  value BLOB NOT NULL,"
                            "  size INTEGER NOT NULL,"
                            "  PRIMARY KEY(key)"
                            ")";
  if (_db != nullptr) {
    return true;
  }

  int r = sqlite3_open(_location.c_str(), &_db);
  if (r != SQLITE_OK) {
    THROW_SQLITE_ERROR(env(), r);
    return false;
  }

  r = sqlite3_exec(_db, sql, 0, 0, nullptr);
  if (r != SQLITE_OK) {
    THROW_SQLITE_ERROR(env(), r);
    return false;
  }

  return true;
}

void Storage::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Realm* realm = Realm::GetCurrent(args);

  if (!args[0]->StrictEquals(realm->isolate_data()->constructor_key_symbol())) {
    THROW_ERR_ILLEGAL_CONSTRUCTOR(env);
    return;
  }

  CHECK(args.IsConstructCall());
  CHECK(args[1]->IsString());
  new Storage(env, args.This(), args[1].As<String>());
}

void Storage::Clear() {
  if (!Open()) {
    return;
  }

  static const char sql[] = "DELETE FROM nodejs_webstorage";
  sqlite3_stmt* stmt = nullptr;
  auto cleanup = OnScopeLeave([&]() { sqlite3_finalize(stmt); });
  CHECK_ERROR_OR_THROW(
      env(), sqlite3_prepare_v2(_db, sql, -1, &stmt, 0), SQLITE_OK, void());
  CHECK_ERROR_OR_THROW(env(), sqlite3_step(stmt), SQLITE_DONE, void());
}

Local<Array> Storage::Enumerate() {
  if (!Open()) {
    return Local<Array>();
  }

  static const char sql[] = "SELECT key FROM nodejs_webstorage";
  sqlite3_stmt* stmt = nullptr;
  auto cleanup = OnScopeLeave([&]() { sqlite3_finalize(stmt); });
  int r = sqlite3_prepare_v2(_db, sql, -1, &stmt, 0);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, Local<Array>());
  std::vector<Local<Value>> values;
  while ((r = sqlite3_step(stmt)) == SQLITE_ROW) {
    CHECK(sqlite3_column_type(stmt, 0) == SQLITE_BLOB);
    values.emplace_back(
        String::NewFromUtf8(
            env()->isolate(),
            reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0)),
            v8::NewStringType::kNormal,
            static_cast<int>(sqlite3_column_bytes(stmt, 0))).ToLocalChecked());
  }
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_DONE, Local<Array>());
  return Array::New(env()->isolate(), values.data(), values.size());
}

Local<Value> Storage::Length() {
  if (!Open()) {
    return Local<Value>();
  }

  static const char sql[] = "SELECT count(*) FROM nodejs_webstorage";
  sqlite3_stmt* stmt = nullptr;
  auto cleanup = OnScopeLeave([&]() { sqlite3_finalize(stmt); });
  int r = sqlite3_prepare_v2(_db, sql, -1, &stmt, 0);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, Local<Value>());
  CHECK_ERROR_OR_THROW(env(), sqlite3_step(stmt), SQLITE_ROW, Local<Value>());
  CHECK(sqlite3_column_type(stmt, 0) == SQLITE_INTEGER);
  int result = sqlite3_column_int(stmt, 0);
  return Integer::New(env()->isolate(), result);
}

Local<Value> Storage::Load(Local<Name> key) {
  if (key->IsSymbol()) {
    auto symbol_map = _symbols.Get(env()->isolate());
    MaybeLocal<Value> result = symbol_map->Get(env()->context(), key);
    return result.FromMaybe(Local<Value>());
  }

  if (!Open()) {
    return Local<Value>();
  }

  static const char sql[] =
      "SELECT value FROM nodejs_webstorage WHERE key = ? LIMIT 1";
  sqlite3_stmt* stmt = nullptr;
  auto cleanup = OnScopeLeave([&]() { sqlite3_finalize(stmt); });
  int r = sqlite3_prepare_v2(_db, sql, -1, &stmt, 0);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, Local<Value>());
  node::Utf8Value utf8key(env()->isolate(), key);
  r = sqlite3_bind_blob(
      stmt, 1, utf8key.out(), utf8key.length(), SQLITE_STATIC);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, Local<Value>());

  auto value = Local<Value>();
  r = sqlite3_step(stmt);
  if (r == SQLITE_ROW) {
    CHECK(sqlite3_column_type(stmt, 0) == SQLITE_BLOB);
    value = String::NewFromUtf8(
        env()->isolate(),
        reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0)),
        v8::NewStringType::kNormal,
        static_cast<int>(sqlite3_column_bytes(stmt, 0))).ToLocalChecked();
  } else if (r != SQLITE_DONE) {
    THROW_SQLITE_ERROR(env(), r);
  }

  return value;
}

Local<Value> Storage::LoadKey(const int index) {
  if (!Open()) {
    return Local<Value>();
  }

  static const char sql[] =
      "SELECT key FROM nodejs_webstorage LIMIT 1 OFFSET ?";
  sqlite3_stmt* stmt = nullptr;
  auto cleanup = OnScopeLeave([&]() { sqlite3_finalize(stmt); });
  int r = sqlite3_prepare_v2(_db, sql, -1, &stmt, 0);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, Local<Value>());
  r = sqlite3_bind_int(stmt, 1, index);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, Local<Value>());

  auto value = Local<Value>();
  r = sqlite3_step(stmt);
  if (r == SQLITE_ROW) {
    CHECK(sqlite3_column_type(stmt, 0) == SQLITE_BLOB);
    value = String::NewFromUtf8(
        env()->isolate(),
        reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0)),
        v8::NewStringType::kNormal,
        static_cast<int>(sqlite3_column_bytes(stmt, 0))).ToLocalChecked();
  } else if (r != SQLITE_DONE) {
    THROW_SQLITE_ERROR(env(), r);
  }

  return value;
}

bool Storage::Remove(Local<Name> key) {
  if (key->IsSymbol()) {
    auto symbol_map = _symbols.Get(env()->isolate());
    Maybe<bool> result = symbol_map->Delete(env()->context(), key);
    return !result.IsNothing();
  }

  if (!Open()) {
    return false;
  }

  // TODO(cjihrig): Use RETURNING with DELETE when broadcasting is implemented.
  static const char sql[] = "DELETE FROM nodejs_webstorage WHERE key = ?";
  sqlite3_stmt* stmt = nullptr;
  auto cleanup = OnScopeLeave([&]() { sqlite3_finalize(stmt); });
  int r = sqlite3_prepare_v2(_db, sql, -1, &stmt, 0);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  node::Utf8Value utf8key(env()->isolate(), key);
  r = sqlite3_bind_blob(
      stmt, 1, utf8key.out(), utf8key.length(), SQLITE_STATIC);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  CHECK_ERROR_OR_THROW(env(), sqlite3_step(stmt), SQLITE_DONE, false);
  return true;
}

bool Storage::Store(Local<Name> key, Local<Value> value) {
  if (key->IsSymbol()) {
    auto symbol_map = _symbols.Get(env()->isolate());
    MaybeLocal<Map> result = symbol_map->Set(env()->context(), key, value);
    return !result.IsEmpty();
  }

  Local<String> val;
  if (!value->ToString(env()->context()).ToLocal(&val)) {
    return false;
  }

  if (!Open()) {
    return false;
  }

  static const char lookup[] =
      "SELECT total_size, value FROM"
      "  (SELECT SUM(size) AS total_size FROM nodejs_webstorage) t1"
      "  FULL OUTER JOIN"
      "  (SELECT value FROM nodejs_webstorage WHERE key = ? LIMIT 1) t2";
  static const char upsert[] =
      "INSERT OR REPLACE INTO nodejs_webstorage (key, value, size)"
      "  VALUES (?, ?, ?)";
  sqlite3_stmt* stmt = nullptr;
  bool in_transaction = false;
  auto cleanup = OnScopeLeave([&]() {
    if (in_transaction) {
      sqlite3_exec(_db, "ROLLBACK", 0, 0, nullptr);
    }

    sqlite3_finalize(stmt);
  });
  int r = sqlite3_exec(_db, "BEGIN TRANSACTION", 0, 0, nullptr);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  in_transaction = true;
  r = sqlite3_prepare_v2(_db, lookup, -1, &stmt, 0);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  node::Utf8Value utf8key(env()->isolate(), key);
  node::Utf8Value utf8val(env()->isolate(), val);
  r = sqlite3_bind_blob(
      stmt, 1, utf8key.out(), utf8key.length(), SQLITE_STATIC);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  r = sqlite3_step(stmt);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_ROW, false);
  sqlite3_int64 total_size = sqlite3_column_int64(stmt, 0);
  int existing_size = sqlite3_column_bytes(stmt, 1);
  sqlite3_int64 new_size = total_size - existing_size + utf8val.length();

  if (sqlite3_column_blob(stmt, 1) == nullptr) {
    new_size += utf8key.length();
  }

  if (new_size > kMaxStorageSize) {
    ThrowQuotaExceededException(env()->context());
    return false;
  }

  r = sqlite3_finalize(stmt);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  r = sqlite3_prepare_v2(_db, upsert, -1, &stmt, 0);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  r = sqlite3_bind_blob(
      stmt, 1, utf8key.out(), utf8key.length(), SQLITE_STATIC);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  r = sqlite3_bind_blob(
      stmt, 2, utf8val.out(), utf8val.length(), SQLITE_STATIC);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  r = sqlite3_bind_int64(
      stmt, 3, utf8key.length() + utf8val.length());
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  CHECK_ERROR_OR_THROW(env(), sqlite3_step(stmt), SQLITE_DONE, false);
  r = sqlite3_exec(_db, "COMMIT", 0, 0, nullptr);
  CHECK_ERROR_OR_THROW(env(), r, SQLITE_OK, false);
  in_transaction = false;
  return true;
}

static Local<Name> Uint32ToName(Local<Context> context, uint32_t index) {
  return Uint32::New(context->GetIsolate(), index)->ToString(context)
      .ToLocalChecked();
}

static void Clear(const FunctionCallbackInfo<Value>& info) {
  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());
  storage->Clear();
}

static void GetItem(const FunctionCallbackInfo<Value>& info) {
  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());
  Environment* env = Environment::GetCurrent(info);

  if (info.Length() < 1) {
    env->ThrowTypeError(
        "Failed to execute 'getItem' on 'Storage': 1 argument required");
    return;
  }

  Local<String> prop;
  if (!info[0]->ToString(env->context()).ToLocal(&prop)) {
    return;
  }

  Local<Value> result = storage->Load(prop);
  if (result.IsEmpty()) {
    info.GetReturnValue().Set(Null(env->isolate()));
  } else {
    info.GetReturnValue().Set(result);
  }
}

static void Key(const FunctionCallbackInfo<Value>& info) {
  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());
  Environment* env = Environment::GetCurrent(info);
  int index;

  if (info.Length() < 1) {
    env->ThrowTypeError(
        "Failed to execute 'key' on 'Storage': 1 argument required");
    return;
  }

  if (!info[0]->Int32Value(env->context()).To(&index)) {
    return;
  }

  if (index < 0) {
    info.GetReturnValue().Set(Null(env->isolate()));
    return;
  }

  Local<Value> result = storage->LoadKey(index);
  if (result.IsEmpty()) {
    info.GetReturnValue().Set(Null(env->isolate()));
  } else {
    info.GetReturnValue().Set(result);
  }
}

static void RemoveItem(const FunctionCallbackInfo<Value>& info) {
  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());
  Environment* env = Environment::GetCurrent(info);
  Local<String> prop;

  if (info.Length() < 1) {
    env->ThrowTypeError(
        "Failed to execute 'removeItem' on 'Storage': 1 argument required");
    return;
  }

  if (!info[0]->ToString(env->context()).ToLocal(&prop)) {
    return;
  }

  storage->Remove(prop);
}

static void SetItem(const FunctionCallbackInfo<Value>& info) {
  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());
  Environment* env = Environment::GetCurrent(info);

  if (info.Length() < 2) {
    env->ThrowTypeError(
        "Failed to execute 'setItem' on 'Storage': 2 arguments required");
    return;
  }

  Local<String> prop;
  if (!info[0]->ToString(env->context()).ToLocal(&prop)) {
    return;
  }

  storage->Store(prop, info[1]);
}

template <typename T>
static bool ShouldIntercept(Local<Name> property,
                            const PropertyCallbackInfo<T>& info) {
  Environment* env = Environment::GetCurrent(info);
  Local<Value> proto = info.Holder()->GetPrototype();

  if (proto->IsObject()) {
    bool has_prop;

    if (!proto.As<Object>()->Has(env->context(), property).To(&has_prop)) {
      return false;
    }

    if (has_prop) {
      return false;
    }
  }

  return true;
}

static void StorageGetter(Local<Name> property,
                          const PropertyCallbackInfo<Value>& info) {
  if (!ShouldIntercept(property, info)) {
    return;
  }

  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());
  Local<Value> result = storage->Load(property);

  if (result.IsEmpty()) {
    info.GetReturnValue().SetUndefined();
  } else {
    info.GetReturnValue().Set(result);
  }
}

static void StorageSetter(Local<Name> property,
                          Local<Value> value,
                          const PropertyCallbackInfo<Value>& info) {
  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());

  if (storage->Store(property, value)) {
    info.GetReturnValue().Set(value);
  }
}

static void StorageQuery(Local<Name> property,
                         const PropertyCallbackInfo<Integer>& info) {
  if (!ShouldIntercept(property, info)) {
    return;
  }

  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());
  Local<Value> result = storage->Load(property);
  if (!result.IsEmpty()) {
    info.GetReturnValue().Set(0);
  }
}

static void StorageDeleter(Local<Name> property,
                           const PropertyCallbackInfo<Boolean>& info) {
  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());

  if (storage->Remove(property)) {
    info.GetReturnValue().Set(true);
  }
}

static void StorageEnumerator(const PropertyCallbackInfo<Array>& info) {
  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());
  info.GetReturnValue().Set(storage->Enumerate());
}

static void StorageDefiner(Local<Name> property,
                           const PropertyDescriptor& desc,
                           const PropertyCallbackInfo<Value>& info) {
  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.Holder());

  if (desc.has_value()) {
    return StorageSetter(property, desc.value(), info);
  }
}

static void IndexedStorageGetter(uint32_t index,
                                 const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  StorageGetter(Uint32ToName(env->context(), index), info);
}

static void IndexedStorageSetter(uint32_t index,
                                 Local<Value> value,
                                 const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  StorageSetter(Uint32ToName(env->context(), index), value, info);
}

static void IndexedStorageQuery(uint32_t index,
                                const PropertyCallbackInfo<Integer>& info) {
  Environment* env = Environment::GetCurrent(info);
  StorageQuery(Uint32ToName(env->context(), index), info);
}

static void IndexedStorageDefiner(uint32_t index,
                                  const PropertyDescriptor& desc,
                                  const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  StorageDefiner(Uint32ToName(env->context(), index), desc, info);
}

static void IndexedStorageDeleter(uint32_t index,
                                  const PropertyCallbackInfo<Boolean>& info) {
  Environment* env = Environment::GetCurrent(info);
  StorageDeleter(Uint32ToName(env->context(), index), info);
}

static void StorageLengthGetter(const FunctionCallbackInfo<Value>& info) {
  Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(&storage, info.This());
  info.GetReturnValue().Set(storage->Length());
}

static void StorageLengthSetter(const FunctionCallbackInfo<Value>& info) {
  info.GetReturnValue().SetUndefined();
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  auto ctor_tmpl = NewFunctionTemplate(isolate, Storage::New);
  auto inst_tmpl = ctor_tmpl->InstanceTemplate();

  inst_tmpl->SetInternalFieldCount(Storage::kInternalFieldCount);
  inst_tmpl->SetHandler(NamedPropertyHandlerConfiguration(
      StorageGetter,
      StorageSetter,
      StorageQuery,
      StorageDeleter,
      StorageEnumerator,
      StorageDefiner,
      nullptr,
      Local<Value>(),
      PropertyHandlerFlags::kHasNoSideEffect));
  inst_tmpl->SetHandler(IndexedPropertyHandlerConfiguration(
      IndexedStorageGetter,
      IndexedStorageSetter,
      IndexedStorageQuery,
      IndexedStorageDeleter,
      nullptr,
      IndexedStorageDefiner,
      nullptr,
      Local<Value>(),
      PropertyHandlerFlags::kHasNoSideEffect));

  Local<FunctionTemplate> length_getter = FunctionTemplate::New(
      isolate, StorageLengthGetter, Local<Value>());
  Local<FunctionTemplate> length_setter = FunctionTemplate::New(
      isolate, StorageLengthSetter, Local<Value>());
  ctor_tmpl->PrototypeTemplate()->SetAccessorProperty(
      FIXED_ONE_BYTE_STRING(isolate, "length"),
      length_getter,
      length_setter,
      DontDelete);

  SetProtoMethod(isolate, ctor_tmpl, "clear", Clear);
  SetProtoMethodNoSideEffect(isolate, ctor_tmpl, "getItem", GetItem);
  SetProtoMethodNoSideEffect(isolate, ctor_tmpl, "key", Key);
  SetProtoMethod(isolate, ctor_tmpl, "removeItem", RemoveItem);
  SetProtoMethod(isolate, ctor_tmpl, "setItem", SetItem);
  SetConstructorFunction(context, target, "Storage", ctor_tmpl);

  auto symbol = env->isolate_data()->constructor_key_symbol();
  target->Set(
      context, FIXED_ONE_BYTE_STRING(isolate, "kConstructorKey"), symbol)
      .Check();
}

}  // namespace webstorage
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(webstorage, node::webstorage::Initialize)
