#include "node_sqlite.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_mem-inl.h"
#include "sqlite3.h"
#include "util-inl.h"

#include <cinttypes>

namespace node {
namespace sqlite {

using v8::Array;
using v8::ArrayBuffer;
using v8::BigInt;
using v8::Boolean;
using v8::ConstructorBehavior;
using v8::Context;
using v8::DontDelete;
using v8::Exception;
using v8::Function;
using v8::FunctionCallback;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::Name;
using v8::NewStringType;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::SideEffectType;
using v8::String;
using v8::Uint8Array;
using v8::Value;

#define CHECK_ERROR_OR_THROW(isolate, db, expr, expected, ret)                 \
  do {                                                                         \
    int r_ = (expr);                                                           \
    if (r_ != (expected)) {                                                    \
      THROW_ERR_SQLITE_ERROR((isolate), (db));                                 \
      return (ret);                                                            \
    }                                                                          \
  } while (0)

#define THROW_AND_RETURN_ON_BAD_STATE(env, condition, msg)                     \
  do {                                                                         \
    if ((condition)) {                                                         \
      THROW_ERR_INVALID_STATE((env), (msg));                                   \
      return;                                                                  \
    }                                                                          \
  } while (0)

inline MaybeLocal<Object> CreateSQLiteError(Isolate* isolate,
                                            const char* message) {
  Local<String> js_msg;
  Local<Object> e;
  if (!String::NewFromUtf8(isolate, message).ToLocal(&js_msg) ||
      !Exception::Error(js_msg)
           ->ToObject(isolate->GetCurrentContext())
           .ToLocal(&e) ||
      e->Set(isolate->GetCurrentContext(),
             OneByteString(isolate, "code"),
             OneByteString(isolate, "ERR_SQLITE_ERROR"))
          .IsNothing()) {
    return MaybeLocal<Object>();
  }
  return e;
}

inline MaybeLocal<Object> CreateSQLiteError(Isolate* isolate, sqlite3* db) {
  int errcode = sqlite3_extended_errcode(db);
  const char* errstr = sqlite3_errstr(errcode);
  const char* errmsg = sqlite3_errmsg(db);
  Local<String> js_errmsg;
  Local<Object> e;
  if (!String::NewFromUtf8(isolate, errstr).ToLocal(&js_errmsg) ||
      !CreateSQLiteError(isolate, errmsg).ToLocal(&e) ||
      e->Set(isolate->GetCurrentContext(),
             OneByteString(isolate, "errcode"),
             Integer::New(isolate, errcode))
          .IsNothing() ||
      e->Set(isolate->GetCurrentContext(),
             OneByteString(isolate, "errstr"),
             js_errmsg)
          .IsNothing()) {
    return MaybeLocal<Object>();
  }
  return e;
}

inline void THROW_ERR_SQLITE_ERROR(Isolate* isolate, sqlite3* db) {
  Local<Object> e;
  if (CreateSQLiteError(isolate, db).ToLocal(&e)) {
    isolate->ThrowException(e);
  }
}

inline void THROW_ERR_SQLITE_ERROR(Isolate* isolate, const char* message) {
  Local<Object> e;
  if (CreateSQLiteError(isolate, message).ToLocal(&e)) {
    isolate->ThrowException(e);
  }
}

DatabaseSync::DatabaseSync(Environment* env,
                           Local<Object> object,
                           DatabaseOpenConfiguration&& open_config,
                           bool open)
    : BaseObject(env, object), open_config_(std::move(open_config)) {
  MakeWeak();
  connection_ = nullptr;

  if (open) {
    Open();
  }
}

void DatabaseSync::DeleteSessions() {
  // all attached sessions need to be deleted before the database is closed
  // https://www.sqlite.org/session/sqlite3session_create.html
  for (auto* session : sessions_) {
    sqlite3session_delete(session);
  }
  sessions_.clear();
}

DatabaseSync::~DatabaseSync() {
  if (IsOpen()) {
    FinalizeStatements();
    DeleteSessions();
    sqlite3_close_v2(connection_);
    connection_ = nullptr;
  }
}

void DatabaseSync::MemoryInfo(MemoryTracker* tracker) const {
  // TODO(tniessen): more accurately track the size of all fields
  tracker->TrackFieldWithSize(
      "open_config", sizeof(open_config_), "DatabaseOpenConfiguration");
}

bool DatabaseSync::Open() {
  if (IsOpen()) {
    THROW_ERR_INVALID_STATE(env(), "database is already open");
    return false;
  }

  // TODO(cjihrig): Support additional flags.
  int flags = open_config_.get_read_only()
                  ? SQLITE_OPEN_READONLY
                  : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
  int r = sqlite3_open_v2(
      open_config_.location().c_str(), &connection_, flags, nullptr);
  CHECK_ERROR_OR_THROW(env()->isolate(), connection_, r, SQLITE_OK, false);

  r = sqlite3_db_config(connection_,
                        SQLITE_DBCONFIG_DQS_DML,
                        static_cast<int>(open_config_.get_enable_dqs()),
                        nullptr);
  CHECK_ERROR_OR_THROW(env()->isolate(), connection_, r, SQLITE_OK, false);
  r = sqlite3_db_config(connection_,
                        SQLITE_DBCONFIG_DQS_DDL,
                        static_cast<int>(open_config_.get_enable_dqs()),
                        nullptr);
  CHECK_ERROR_OR_THROW(env()->isolate(), connection_, r, SQLITE_OK, false);

  int foreign_keys_enabled;
  r = sqlite3_db_config(
      connection_,
      SQLITE_DBCONFIG_ENABLE_FKEY,
      static_cast<int>(open_config_.get_enable_foreign_keys()),
      &foreign_keys_enabled);
  CHECK_ERROR_OR_THROW(env()->isolate(), connection_, r, SQLITE_OK, false);
  CHECK_EQ(foreign_keys_enabled, open_config_.get_enable_foreign_keys());

  return true;
}

void DatabaseSync::FinalizeStatements() {
  for (auto stmt : statements_) {
    stmt->Finalize();
  }

  statements_.clear();
}

void DatabaseSync::UntrackStatement(StatementSync* statement) {
  auto it = statements_.find(statement);
  if (it != statements_.end()) {
    statements_.erase(it);
  }
}

inline bool DatabaseSync::IsOpen() {
  return connection_ != nullptr;
}

inline sqlite3* DatabaseSync::Connection() {
  return connection_;
}

void DatabaseSync::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args.IsConstructCall()) {
    THROW_ERR_CONSTRUCT_CALL_REQUIRED(env);
    return;
  }

  if (!args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"path\" argument must be a string.");
    return;
  }

  std::string location =
      Utf8Value(env->isolate(), args[0].As<String>()).ToString();
  DatabaseOpenConfiguration open_config(std::move(location));

  bool open = true;

  if (args.Length() > 1) {
    if (!args[1]->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                                 "The \"options\" argument must be an object.");
      return;
    }

    Local<Object> options = args[1].As<Object>();
    Local<String> open_string = FIXED_ONE_BYTE_STRING(env->isolate(), "open");
    Local<Value> open_v;
    if (!options->Get(env->context(), open_string).ToLocal(&open_v)) {
      return;
    }
    if (!open_v->IsUndefined()) {
      if (!open_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(), "The \"options.open\" argument must be a boolean.");
        return;
      }
      open = open_v.As<Boolean>()->Value();
    }

    Local<String> read_only_string =
        FIXED_ONE_BYTE_STRING(env->isolate(), "readOnly");
    Local<Value> read_only_v;
    if (!options->Get(env->context(), read_only_string).ToLocal(&read_only_v)) {
      return;
    }
    if (!read_only_v->IsUndefined()) {
      if (!read_only_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.readOnly\" argument must be a boolean.");
        return;
      }
      open_config.set_read_only(read_only_v.As<Boolean>()->Value());
    }

    Local<String> enable_foreign_keys_string =
        FIXED_ONE_BYTE_STRING(env->isolate(), "enableForeignKeyConstraints");
    Local<Value> enable_foreign_keys_v;
    if (!options->Get(env->context(), enable_foreign_keys_string)
             .ToLocal(&enable_foreign_keys_v)) {
      return;
    }
    if (!enable_foreign_keys_v->IsUndefined()) {
      if (!enable_foreign_keys_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.enableForeignKeyConstraints\" argument must be a "
            "boolean.");
        return;
      }
      open_config.set_enable_foreign_keys(
          enable_foreign_keys_v.As<Boolean>()->Value());
    }

    Local<String> enable_dqs_string = FIXED_ONE_BYTE_STRING(
        env->isolate(), "enableDoubleQuotedStringLiterals");
    Local<Value> enable_dqs_v;
    if (!options->Get(env->context(), enable_dqs_string)
             .ToLocal(&enable_dqs_v)) {
      return;
    }
    if (!enable_dqs_v->IsUndefined()) {
      if (!enable_dqs_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.enableDoubleQuotedStringLiterals\" argument must be "
            "a boolean.");
        return;
      }
      open_config.set_enable_dqs(enable_dqs_v.As<Boolean>()->Value());
    }
  }

  new DatabaseSync(env, args.This(), std::move(open_config), open);
}

void DatabaseSync::Open(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  db->Open();
}

void DatabaseSync::Close(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");
  db->FinalizeStatements();
  db->DeleteSessions();
  int r = sqlite3_close_v2(db->connection_);
  CHECK_ERROR_OR_THROW(env->isolate(), db->connection_, r, SQLITE_OK, void());
  db->connection_ = nullptr;
}

void DatabaseSync::Prepare(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");

  if (!args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"sql\" argument must be a string.");
    return;
  }

  Utf8Value sql(env->isolate(), args[0].As<String>());
  sqlite3_stmt* s = nullptr;
  int r = sqlite3_prepare_v2(db->connection_, *sql, -1, &s, 0);
  CHECK_ERROR_OR_THROW(env->isolate(), db->connection_, r, SQLITE_OK, void());
  BaseObjectPtr<StatementSync> stmt = StatementSync::Create(env, db, s);
  db->statements_.insert(stmt.get());
  args.GetReturnValue().Set(stmt->object());
}

void DatabaseSync::Exec(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");

  if (!args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"sql\" argument must be a string.");
    return;
  }

  Utf8Value sql(env->isolate(), args[0].As<String>());
  int r = sqlite3_exec(db->connection_, *sql, nullptr, nullptr, nullptr);
  CHECK_ERROR_OR_THROW(env->isolate(), db->connection_, r, SQLITE_OK, void());
}

void DatabaseSync::CreateSession(const FunctionCallbackInfo<Value>& args) {
  std::string table;
  std::string db_name = "main";

  Environment* env = Environment::GetCurrent(args);
  if (args.Length() > 0) {
    if (!args[0]->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                                 "The \"options\" argument must be an object.");
      return;
    }

    Local<Object> options = args[0].As<Object>();

    Local<String> table_key = FIXED_ONE_BYTE_STRING(env->isolate(), "table");
    if (options->HasOwnProperty(env->context(), table_key).FromJust()) {
      Local<Value> table_value;
      if (!options->Get(env->context(), table_key).ToLocal(&table_value)) {
        return;
      }

      if (table_value->IsString()) {
        String::Utf8Value str(env->isolate(), table_value);
        table = *str;
      } else {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(), "The \"options.table\" argument must be a string.");
        return;
      }
    }

    Local<String> db_key =
        String::NewFromUtf8(env->isolate(), "db", NewStringType::kNormal)
            .ToLocalChecked();
    if (options->HasOwnProperty(env->context(), db_key).FromJust()) {
      Local<Value> db_value =
          options->Get(env->context(), db_key).ToLocalChecked();
      if (db_value->IsString()) {
        String::Utf8Value str(env->isolate(), db_value);
        db_name = std::string(*str);
      } else {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(), "The \"options.db\" argument must be a string.");
        return;
      }
    }
  }

  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");

  sqlite3_session* pSession;
  int r = sqlite3session_create(db->connection_, db_name.c_str(), &pSession);
  CHECK_ERROR_OR_THROW(env->isolate(), db->connection_, r, SQLITE_OK, void());
  db->sessions_.insert(pSession);

  r = sqlite3session_attach(pSession, table == "" ? nullptr : table.c_str());
  CHECK_ERROR_OR_THROW(env->isolate(), db->connection_, r, SQLITE_OK, void());

  BaseObjectPtr<Session> session =
      Session::Create(env, BaseObjectWeakPtr<DatabaseSync>(db), pSession);
  args.GetReturnValue().Set(session->object());
}

// the reason for using static functions here is that SQLite needs a
// function pointer
static std::function<int()> conflictCallback;

static int xConflict(void* pCtx, int eConflict, sqlite3_changeset_iter* pIter) {
  if (!conflictCallback) return SQLITE_CHANGESET_ABORT;
  return conflictCallback();
}

static std::function<bool(std::string)> filterCallback;

static int xFilter(void* pCtx, const char* zTab) {
  if (!filterCallback) return 1;

  return filterCallback(zTab) ? 1 : 0;
}

void DatabaseSync::ApplyChangeset(const FunctionCallbackInfo<Value>& args) {
  conflictCallback = nullptr;
  filterCallback = nullptr;

  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");

  if (!args[0]->IsUint8Array()) {
    THROW_ERR_INVALID_ARG_TYPE(
        env->isolate(), "The \"changeset\" argument must be a Uint8Array.");
    return;
  }

  if (args.Length() > 1 && !args[1]->IsUndefined()) {
    if (!args[1]->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                                 "The \"options\" argument must be an object.");
      return;
    }

    Local<Object> options = args[1].As<Object>();
    Local<Value> conflictValue =
        options->Get(env->context(), env->onconflict_string()).ToLocalChecked();

    if (!conflictValue->IsUndefined()) {
      if (!conflictValue->IsNumber()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.onConflict\" argument must be a number.");
        return;
      }

      int conflictInt = conflictValue->Int32Value(env->context()).FromJust();
      conflictCallback = [conflictInt]() -> int { return conflictInt; };
    }

    if (options->HasOwnProperty(env->context(), env->filter_string())
            .FromJust()) {
      Local<Value> filterValue =
          options->Get(env->context(), env->filter_string()).ToLocalChecked();

      if (!filterValue->IsFunction()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.filter\" argument must be a function.");
        return;
      }

      Local<Function> filterFunc = filterValue.As<Function>();

      filterCallback = [env, filterFunc](std::string item) -> bool {
        Local<Value> argv[] = {String::NewFromUtf8(env->isolate(),
                                                   item.c_str(),
                                                   NewStringType::kNormal)
                                   .ToLocalChecked()};
        Local<Value> result =
            filterFunc->Call(env->context(), Null(env->isolate()), 1, argv)
                .ToLocalChecked();
        return result->BooleanValue(env->isolate());
      };
    }
  }

  ArrayBufferViewContents<uint8_t> buf(args[0]);
  int r = sqlite3changeset_apply(
      db->connection_,
      buf.length(),
      const_cast<void*>(static_cast<const void*>(buf.data())),
      xFilter,
      xConflict,
      nullptr);
  if (r == SQLITE_ABORT) {
    args.GetReturnValue().Set(false);
    return;
  }
  CHECK_ERROR_OR_THROW(env->isolate(), db->connection_, r, SQLITE_OK, void());
  args.GetReturnValue().Set(true);
}

StatementSync::StatementSync(Environment* env,
                             Local<Object> object,
                             DatabaseSync* db,
                             sqlite3_stmt* stmt)
    : BaseObject(env, object) {
  MakeWeak();
  db_ = db;
  statement_ = stmt;
  // In the future, some of these options could be set at the database
  // connection level and inherited by statements to reduce boilerplate.
  use_big_ints_ = false;
  allow_bare_named_params_ = true;
  bare_named_params_ = std::nullopt;
}

StatementSync::~StatementSync() {
  if (!IsFinalized()) {
    db_->UntrackStatement(this);
    Finalize();
  }
}

void StatementSync::Finalize() {
  sqlite3_finalize(statement_);
  statement_ = nullptr;
}

inline bool StatementSync::IsFinalized() {
  return statement_ == nullptr;
}

bool StatementSync::BindParams(const FunctionCallbackInfo<Value>& args) {
  int r = sqlite3_clear_bindings(statement_);
  CHECK_ERROR_OR_THROW(
      env()->isolate(), db_->Connection(), r, SQLITE_OK, false);

  int anon_idx = 1;
  int anon_start = 0;

  if (args[0]->IsObject() && !args[0]->IsUint8Array()) {
    Local<Object> obj = args[0].As<Object>();
    Local<Context> context = obj->GetIsolate()->GetCurrentContext();
    Local<Array> keys;
    if (!obj->GetOwnPropertyNames(context).ToLocal(&keys)) {
      return false;
    }

    if (allow_bare_named_params_ && !bare_named_params_.has_value()) {
      bare_named_params_.emplace();
      int param_count = sqlite3_bind_parameter_count(statement_);
      // Parameter indexing starts at one.
      for (int i = 1; i <= param_count; ++i) {
        const char* name = sqlite3_bind_parameter_name(statement_, i);
        if (name == nullptr) {
          continue;
        }

        auto bare_name = std::string(name + 1);
        auto full_name = std::string(name);
        auto insertion = bare_named_params_->insert({bare_name, full_name});
        if (insertion.second == false) {
          auto existing_full_name = (*insertion.first).second;
          if (full_name != existing_full_name) {
            THROW_ERR_INVALID_STATE(
                env(),
                "Cannot create bare named parameter '%s' because of "
                "conflicting names '%s' and '%s'.",
                bare_name,
                existing_full_name,
                full_name);
            return false;
          }
        }
      }
    }

    uint32_t len = keys->Length();
    for (uint32_t j = 0; j < len; j++) {
      Local<Value> key;
      if (!keys->Get(context, j).ToLocal(&key)) {
        return false;
      }

      Utf8Value utf8_key(env()->isolate(), key);
      int r = sqlite3_bind_parameter_index(statement_, *utf8_key);
      if (r == 0) {
        if (allow_bare_named_params_) {
          auto lookup = bare_named_params_->find(std::string(*utf8_key));
          if (lookup != bare_named_params_->end()) {
            r = sqlite3_bind_parameter_index(statement_,
                                             lookup->second.c_str());
          }
        }

        if (r == 0) {
          THROW_ERR_INVALID_STATE(
              env(), "Unknown named parameter '%s'", *utf8_key);
          return false;
        }
      }

      Local<Value> value;
      if (!obj->Get(context, key).ToLocal(&value)) {
        return false;
      }

      if (!BindValue(value, r)) {
        return false;
      }
    }
    anon_start++;
  }

  for (int i = anon_start; i < args.Length(); ++i) {
    while (sqlite3_bind_parameter_name(statement_, anon_idx) != nullptr) {
      anon_idx++;
    }

    if (!BindValue(args[i], anon_idx)) {
      return false;
    }

    anon_idx++;
  }

  return true;
}

bool StatementSync::BindValue(const Local<Value>& value, const int index) {
  // SQLite only supports a subset of JavaScript types. Some JS types such as
  // functions don't make sense to support. Other JS types such as booleans and
  // Dates could be supported by converting them to numbers. However, there
  // would not be a good way to read the values back from SQLite with the
  // original type.
  int r;
  if (value->IsNumber()) {
    double val = value.As<Number>()->Value();
    r = sqlite3_bind_double(statement_, index, val);
  } else if (value->IsString()) {
    Utf8Value val(env()->isolate(), value.As<String>());
    r = sqlite3_bind_text(
        statement_, index, *val, val.length(), SQLITE_TRANSIENT);
  } else if (value->IsNull()) {
    r = sqlite3_bind_null(statement_, index);
  } else if (value->IsUint8Array()) {
    ArrayBufferViewContents<uint8_t> buf(value);
    r = sqlite3_bind_blob(
        statement_, index, buf.data(), buf.length(), SQLITE_TRANSIENT);
  } else if (value->IsBigInt()) {
    bool lossless;
    int64_t as_int = value.As<BigInt>()->Int64Value(&lossless);
    if (!lossless) {
      THROW_ERR_INVALID_ARG_VALUE(env(), "BigInt value is too large to bind.");
      return false;
    }
    r = sqlite3_bind_int64(statement_, index, as_int);
  } else {
    THROW_ERR_INVALID_ARG_TYPE(
        env()->isolate(),
        "Provided value cannot be bound to SQLite parameter %d.",
        index);
    return false;
  }

  CHECK_ERROR_OR_THROW(
      env()->isolate(), db_->Connection(), r, SQLITE_OK, false);
  return true;
}

MaybeLocal<Value> StatementSync::ColumnToValue(const int column) {
  switch (sqlite3_column_type(statement_, column)) {
    case SQLITE_INTEGER: {
      sqlite3_int64 value = sqlite3_column_int64(statement_, column);
      if (use_big_ints_) {
        return BigInt::New(env()->isolate(), value);
      } else if (std::abs(value) <= kMaxSafeJsInteger) {
        return Number::New(env()->isolate(), value);
      } else {
        THROW_ERR_OUT_OF_RANGE(env()->isolate(),
                               "The value of column %d is too large to be "
                               "represented as a JavaScript number: %" PRId64,
                               column,
                               value);
        return MaybeLocal<Value>();
      }
    }
    case SQLITE_FLOAT:
      return Number::New(env()->isolate(),
                         sqlite3_column_double(statement_, column));
    case SQLITE_TEXT: {
      const char* value = reinterpret_cast<const char*>(
          sqlite3_column_text(statement_, column));
      return String::NewFromUtf8(env()->isolate(), value).As<Value>();
    }
    case SQLITE_NULL:
      return Null(env()->isolate());
    case SQLITE_BLOB: {
      size_t size =
          static_cast<size_t>(sqlite3_column_bytes(statement_, column));
      auto data = reinterpret_cast<const uint8_t*>(
          sqlite3_column_blob(statement_, column));
      auto store = ArrayBuffer::NewBackingStore(env()->isolate(), size);
      memcpy(store->Data(), data, size);
      auto ab = ArrayBuffer::New(env()->isolate(), std::move(store));
      return Uint8Array::New(ab, 0, size);
    }
    default:
      UNREACHABLE("Bad SQLite column type");
  }
}

MaybeLocal<Name> StatementSync::ColumnNameToName(const int column) {
  const char* col_name = sqlite3_column_name(statement_, column);
  if (col_name == nullptr) {
    THROW_ERR_INVALID_STATE(env(), "Cannot get name of column %d", column);
    return MaybeLocal<Name>();
  }

  return String::NewFromUtf8(env()->isolate(), col_name).As<Name>();
}

void StatementSync::MemoryInfo(MemoryTracker* tracker) const {}

void StatementSync::All(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  Isolate* isolate = env->isolate();
  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(isolate, stmt->db_->Connection(), r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
  }

  auto reset = OnScopeLeave([&]() { sqlite3_reset(stmt->statement_); });
  int num_cols = sqlite3_column_count(stmt->statement_);
  LocalVector<Value> rows(isolate);
  while ((r = sqlite3_step(stmt->statement_)) == SQLITE_ROW) {
    LocalVector<Name> row_keys(isolate);
    row_keys.reserve(num_cols);
    LocalVector<Value> row_values(isolate);
    row_values.reserve(num_cols);

    for (int i = 0; i < num_cols; ++i) {
      Local<Name> key;
      if (!stmt->ColumnNameToName(i).ToLocal(&key)) return;
      Local<Value> val;
      if (!stmt->ColumnToValue(i).ToLocal(&val)) return;
      row_keys.emplace_back(key);
      row_values.emplace_back(val);
    }

    Local<Object> row = Object::New(
        isolate, Null(isolate), row_keys.data(), row_values.data(), num_cols);
    rows.emplace_back(row);
  }

  CHECK_ERROR_OR_THROW(
      isolate, stmt->db_->Connection(), r, SQLITE_DONE, void());
  args.GetReturnValue().Set(Array::New(isolate, rows.data(), rows.size()));
}

void StatementSync::Get(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  Isolate* isolate = env->isolate();
  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(isolate, stmt->db_->Connection(), r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
  }

  auto reset = OnScopeLeave([&]() { sqlite3_reset(stmt->statement_); });
  r = sqlite3_step(stmt->statement_);
  if (r == SQLITE_DONE) return;
  if (r != SQLITE_ROW) {
    THROW_ERR_SQLITE_ERROR(isolate, stmt->db_->Connection());
    return;
  }

  int num_cols = sqlite3_column_count(stmt->statement_);
  if (num_cols == 0) {
    return;
  }

  LocalVector<Name> keys(isolate);
  keys.reserve(num_cols);
  LocalVector<Value> values(isolate);
  values.reserve(num_cols);

  for (int i = 0; i < num_cols; ++i) {
    Local<Name> key;
    if (!stmt->ColumnNameToName(i).ToLocal(&key)) return;
    Local<Value> val;
    if (!stmt->ColumnToValue(i).ToLocal(&val)) return;
    keys.emplace_back(key);
    values.emplace_back(val);
  }

  Local<Object> result =
      Object::New(isolate, Null(isolate), keys.data(), values.data(), num_cols);

  args.GetReturnValue().Set(result);
}

void StatementSync::Run(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(
      env->isolate(), stmt->db_->Connection(), r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
  }

  auto reset = OnScopeLeave([&]() { sqlite3_reset(stmt->statement_); });
  r = sqlite3_step(stmt->statement_);
  if (r != SQLITE_ROW && r != SQLITE_DONE) {
    THROW_ERR_SQLITE_ERROR(env->isolate(), stmt->db_->Connection());
    return;
  }

  Local<Object> result = Object::New(env->isolate());
  sqlite3_int64 last_insert_rowid =
      sqlite3_last_insert_rowid(stmt->db_->Connection());
  sqlite3_int64 changes = sqlite3_changes64(stmt->db_->Connection());
  Local<Value> last_insert_rowid_val;
  Local<Value> changes_val;

  if (stmt->use_big_ints_) {
    last_insert_rowid_val = BigInt::New(env->isolate(), last_insert_rowid);
    changes_val = BigInt::New(env->isolate(), changes);
  } else {
    last_insert_rowid_val = Number::New(env->isolate(), last_insert_rowid);
    changes_val = Number::New(env->isolate(), changes);
  }

  if (result
          ->Set(env->context(),
                env->last_insert_rowid_string(),
                last_insert_rowid_val)
          .IsNothing() ||
      result->Set(env->context(), env->changes_string(), changes_val)
          .IsNothing()) {
    return;
  }

  args.GetReturnValue().Set(result);
}

void StatementSync::SourceSQLGetter(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  Local<String> sql;
  if (!String::NewFromUtf8(env->isolate(), sqlite3_sql(stmt->statement_))
           .ToLocal(&sql)) {
    return;
  }
  args.GetReturnValue().Set(sql);
}

void StatementSync::ExpandedSQLGetter(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");

  // sqlite3_expanded_sql may return nullptr without producing an error code.
  char* expanded = sqlite3_expanded_sql(stmt->statement_);
  if (expanded == nullptr) {
    return THROW_ERR_SQLITE_ERROR(
        env->isolate(), "Expanded SQL text would exceed configured limits");
  }
  auto maybe_expanded = String::NewFromUtf8(env->isolate(), expanded);
  sqlite3_free(expanded);
  Local<String> result;
  if (!maybe_expanded.ToLocal(&result)) {
    return;
  }
  args.GetReturnValue().Set(result);
}

void StatementSync::SetAllowBareNamedParameters(
    const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");

  if (!args[0]->IsBoolean()) {
    THROW_ERR_INVALID_ARG_TYPE(
        env->isolate(),
        "The \"allowBareNamedParameters\" argument must be a boolean.");
    return;
  }

  stmt->allow_bare_named_params_ = args[0]->IsTrue();
}

void StatementSync::SetReadBigInts(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");

  if (!args[0]->IsBoolean()) {
    THROW_ERR_INVALID_ARG_TYPE(
        env->isolate(), "The \"readBigInts\" argument must be a boolean.");
    return;
  }

  stmt->use_big_ints_ = args[0]->IsTrue();
}

void IllegalConstructor(const FunctionCallbackInfo<Value>& args) {
  THROW_ERR_ILLEGAL_CONSTRUCTOR(Environment::GetCurrent(args));
}

static inline void SetSideEffectFreeGetter(
    Isolate* isolate,
    Local<FunctionTemplate> class_template,
    Local<String> name,
    FunctionCallback fn) {
  Local<FunctionTemplate> getter =
      FunctionTemplate::New(isolate,
                            fn,
                            Local<Value>(),
                            v8::Signature::New(isolate, class_template),
                            /* length */ 0,
                            ConstructorBehavior::kThrow,
                            SideEffectType::kHasNoSideEffect);
  class_template->InstanceTemplate()->SetAccessorProperty(
      name, getter, Local<FunctionTemplate>(), DontDelete);
}

Local<FunctionTemplate> StatementSync::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl =
      env->sqlite_statement_sync_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "StatementSync"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        StatementSync::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "all", StatementSync::All);
    SetProtoMethod(isolate, tmpl, "get", StatementSync::Get);
    SetProtoMethod(isolate, tmpl, "run", StatementSync::Run);
    SetSideEffectFreeGetter(isolate,
                            tmpl,
                            FIXED_ONE_BYTE_STRING(isolate, "sourceSQL"),
                            StatementSync::SourceSQLGetter);
    SetSideEffectFreeGetter(isolate,
                            tmpl,
                            FIXED_ONE_BYTE_STRING(isolate, "expandedSQL"),
                            StatementSync::ExpandedSQLGetter);
    SetProtoMethod(isolate,
                   tmpl,
                   "setAllowBareNamedParameters",
                   StatementSync::SetAllowBareNamedParameters);
    SetProtoMethod(
        isolate, tmpl, "setReadBigInts", StatementSync::SetReadBigInts);
    env->set_sqlite_statement_sync_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<StatementSync> StatementSync::Create(Environment* env,
                                                   DatabaseSync* db,
                                                   sqlite3_stmt* stmt) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return BaseObjectPtr<StatementSync>();
  }

  return MakeBaseObject<StatementSync>(env, obj, db, stmt);
}

Session::Session(Environment* env,
                 Local<Object> object,
                 BaseObjectWeakPtr<DatabaseSync> database,
                 sqlite3_session* session)
    : BaseObject(env, object),
      session_(session),
      database_(std::move(database)) {
  MakeWeak();
}

Session::~Session() {
  Delete();
}

BaseObjectPtr<Session> Session::Create(Environment* env,
                                       BaseObjectWeakPtr<DatabaseSync> database,
                                       sqlite3_session* session) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return BaseObjectPtr<Session>();
  }

  return MakeBaseObject<Session>(env, obj, std::move(database), session);
}

Local<FunctionTemplate> Session::GetConstructorTemplate(Environment* env) {
  Local<FunctionTemplate> tmpl = env->sqlite_session_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Session"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Session::kInternalFieldCount);
    SetProtoMethod(isolate,
                   tmpl,
                   "changeset",
                   Session::Changeset<sqlite3session_changeset>);
    SetProtoMethod(
        isolate, tmpl, "patchset", Session::Changeset<sqlite3session_patchset>);
    SetProtoMethod(isolate, tmpl, "close", Session::Close);
    env->set_sqlite_session_constructor_template(tmpl);
  }
  return tmpl;
}

void Session::MemoryInfo(MemoryTracker* tracker) const {}

template <Sqlite3ChangesetGenFunc sqliteChangesetFunc>
void Session::Changeset(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = Environment::GetCurrent(args);
  sqlite3* db = session->database_ ? session->database_->connection_ : nullptr;
  THROW_AND_RETURN_ON_BAD_STATE(
      env, !session->database_->IsOpen(), "database is not open");
  THROW_AND_RETURN_ON_BAD_STATE(
      env, session->session_ == nullptr, "session is not open");

  int nChangeset;
  void* pChangeset;
  int r = sqliteChangesetFunc(session->session_, &nChangeset, &pChangeset);
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());

  auto freeChangeset = OnScopeLeave([&] { sqlite3_free(pChangeset); });

  Local<ArrayBuffer> buffer = ArrayBuffer::New(env->isolate(), nChangeset);
  std::memcpy(buffer->GetBackingStore()->Data(), pChangeset, nChangeset);
  Local<Uint8Array> uint8Array = Uint8Array::New(buffer, 0, nChangeset);

  args.GetReturnValue().Set(uint8Array);
}

void Session::Close(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, !session->database_->IsOpen(), "database is not open");
  THROW_AND_RETURN_ON_BAD_STATE(
      env, session->session_ == nullptr, "session is not open");

  session->Delete();
}

void Session::Delete() {
  if (!database_ || !database_->connection_ || session_ == nullptr) return;
  sqlite3session_delete(session_);
  database_->sessions_.erase(session_);
  session_ = nullptr;
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> db_tmpl =
      NewFunctionTemplate(isolate, DatabaseSync::New);
  db_tmpl->InstanceTemplate()->SetInternalFieldCount(
      DatabaseSync::kInternalFieldCount);

  SetProtoMethod(isolate, db_tmpl, "open", DatabaseSync::Open);
  SetProtoMethod(isolate, db_tmpl, "close", DatabaseSync::Close);
  SetProtoMethod(isolate, db_tmpl, "prepare", DatabaseSync::Prepare);
  SetProtoMethod(isolate, db_tmpl, "exec", DatabaseSync::Exec);
  SetProtoMethod(
      isolate, db_tmpl, "createSession", DatabaseSync::CreateSession);
  SetProtoMethod(
      isolate, db_tmpl, "applyChangeset", DatabaseSync::ApplyChangeset);
  SetConstructorFunction(context, target, "DatabaseSync", db_tmpl);
  SetConstructorFunction(context,
                         target,
                         "StatementSync",
                         StatementSync::GetConstructorTemplate(env));

  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_OMIT);
  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_REPLACE);
  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_ABORT);
}

}  // namespace sqlite
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(sqlite, node::sqlite::Initialize)
