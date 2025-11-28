#include "node_sqlite.h"
#include <path.h>
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_mem-inl.h"
#include "node_url.h"
#include "sqlite3.h"
#include "threadpoolwork-inl.h"
#include "util-inl.h"

#include <cinttypes>

namespace node {
namespace sqlite {

using v8::Array;
using v8::ArrayBuffer;
using v8::BackingStoreInitializationMode;
using v8::BigInt;
using v8::Boolean;
using v8::ConstructorBehavior;
using v8::Context;
using v8::DictionaryTemplate;
using v8::DontDelete;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::Function;
using v8::FunctionCallback;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::JustVoid;
using v8::Local;
using v8::LocalVector;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::NewStringType;
using v8::Nothing;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::Promise;
using v8::SideEffectType;
using v8::String;
using v8::TryCatch;
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

#define SQLITE_VALUE_TO_JS(from, isolate, use_big_int_args, result, ...)       \
  do {                                                                         \
    switch (sqlite3_##from##_type(__VA_ARGS__)) {                              \
      case SQLITE_INTEGER: {                                                   \
        sqlite3_int64 val = sqlite3_##from##_int64(__VA_ARGS__);               \
        if ((use_big_int_args)) {                                              \
          (result) = BigInt::New((isolate), val);                              \
        } else if (std::abs(val) <= kMaxSafeJsInteger) {                       \
          (result) = Number::New((isolate), val);                              \
        } else {                                                               \
          THROW_ERR_OUT_OF_RANGE((isolate),                                    \
                                 "Value is too large to be represented as a "  \
                                 "JavaScript number: %" PRId64,                \
                                 val);                                         \
        }                                                                      \
        break;                                                                 \
      }                                                                        \
      case SQLITE_FLOAT: {                                                     \
        (result) =                                                             \
            Number::New((isolate), sqlite3_##from##_double(__VA_ARGS__));      \
        break;                                                                 \
      }                                                                        \
      case SQLITE_TEXT: {                                                      \
        const char* v =                                                        \
            reinterpret_cast<const char*>(sqlite3_##from##_text(__VA_ARGS__)); \
        (result) = String::NewFromUtf8((isolate), v).As<Value>();              \
        break;                                                                 \
      }                                                                        \
      case SQLITE_NULL: {                                                      \
        (result) = Null((isolate));                                            \
        break;                                                                 \
      }                                                                        \
      case SQLITE_BLOB: {                                                      \
        size_t size =                                                          \
            static_cast<size_t>(sqlite3_##from##_bytes(__VA_ARGS__));          \
        auto data = reinterpret_cast<const uint8_t*>(                          \
            sqlite3_##from##_blob(__VA_ARGS__));                               \
        auto store = ArrayBuffer::NewBackingStore(                             \
            (isolate), size, BackingStoreInitializationMode::kUninitialized);  \
        memcpy(store->Data(), data, size);                                     \
        auto ab = ArrayBuffer::New((isolate), std::move(store));               \
        (result) = Uint8Array::New(ab, 0, size);                               \
        break;                                                                 \
      }                                                                        \
      default:                                                                 \
        UNREACHABLE("Bad SQLite value");                                       \
    }                                                                          \
  } while (0)

namespace {
Local<DictionaryTemplate> getLazyIterTemplate(Environment* env) {
  auto iter_template = env->iter_template();
  if (iter_template.IsEmpty()) {
    static constexpr std::string_view iter_keys[] = {"done", "value"};
    iter_template = DictionaryTemplate::New(env->isolate(), iter_keys);
    env->set_iter_template(iter_template);
  }
  return iter_template;
}
}  // namespace

inline MaybeLocal<Object> CreateSQLiteError(Isolate* isolate,
                                            const char* message) {
  Local<String> js_msg;
  Local<Object> e;
  Environment* env = Environment::GetCurrent(isolate);
  if (!String::NewFromUtf8(isolate, message).ToLocal(&js_msg) ||
      !Exception::Error(js_msg)
           ->ToObject(isolate->GetCurrentContext())
           .ToLocal(&e) ||
      e->Set(isolate->GetCurrentContext(),
             env->code_string(),
             env->err_sqlite_error_string())
          .IsNothing()) {
    return MaybeLocal<Object>();
  }
  return e;
}

inline MaybeLocal<Object> CreateSQLiteError(Isolate* isolate, int errcode) {
  const char* errstr = sqlite3_errstr(errcode);
  Local<String> js_errmsg;
  Local<Object> e;
  Environment* env = Environment::GetCurrent(isolate);
  if (!String::NewFromUtf8(isolate, errstr).ToLocal(&js_errmsg) ||
      !CreateSQLiteError(isolate, errstr).ToLocal(&e) ||
      e->Set(env->context(),
             env->errcode_string(),
             Integer::New(isolate, errcode))
          .IsNothing() ||
      e->Set(env->context(), env->errstr_string(), js_errmsg).IsNothing()) {
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
  Environment* env = Environment::GetCurrent(isolate);
  if (!String::NewFromUtf8(isolate, errstr).ToLocal(&js_errmsg) ||
      !CreateSQLiteError(isolate, errmsg).ToLocal(&e) ||
      e->Set(isolate->GetCurrentContext(),
             env->errcode_string(),
             Integer::New(isolate, errcode))
          .IsNothing() ||
      e->Set(isolate->GetCurrentContext(), env->errstr_string(), js_errmsg)
          .IsNothing()) {
    return MaybeLocal<Object>();
  }
  return e;
}

void JSValueToSQLiteResult(Isolate* isolate,
                           sqlite3_context* ctx,
                           Local<Value> value) {
  if (value->IsNullOrUndefined()) {
    sqlite3_result_null(ctx);
  } else if (value->IsNumber()) {
    sqlite3_result_double(ctx, value.As<Number>()->Value());
  } else if (value->IsString()) {
    Utf8Value val(isolate, value.As<String>());
    sqlite3_result_text(ctx, *val, val.length(), SQLITE_TRANSIENT);
  } else if (value->IsArrayBufferView()) {
    ArrayBufferViewContents<uint8_t> buf(value);
    sqlite3_result_blob(ctx, buf.data(), buf.length(), SQLITE_TRANSIENT);
  } else if (value->IsBigInt()) {
    bool lossless;
    int64_t as_int = value.As<BigInt>()->Int64Value(&lossless);
    if (!lossless) {
      sqlite3_result_error(ctx, "BigInt value is too large for SQLite", -1);
      return;
    }
    sqlite3_result_int64(ctx, as_int);
  } else if (value->IsPromise()) {
    sqlite3_result_error(
        ctx, "Asynchronous user-defined functions are not supported", -1);
  } else {
    sqlite3_result_error(
        ctx,
        "Returned JavaScript value cannot be converted to a SQLite value",
        -1);
  }
}

class Database;

inline void THROW_ERR_SQLITE_ERROR(Isolate* isolate, Database* db) {
  if (db->ShouldIgnoreSQLiteError()) {
    db->SetIgnoreNextSQLiteError(false);
    return;
  }

  Local<Object> e;
  if (CreateSQLiteError(isolate, db->Connection()).ToLocal(&e)) {
    isolate->ThrowException(e);
  }
}

inline void THROW_ERR_SQLITE_ERROR(Isolate* isolate, const char* message) {
  Local<Object> e;
  if (CreateSQLiteError(isolate, message).ToLocal(&e)) {
    isolate->ThrowException(e);
  }
}

inline void THROW_ERR_SQLITE_ERROR(Isolate* isolate, int errcode) {
  const char* errstr = sqlite3_errstr(errcode);

  Environment* env = Environment::GetCurrent(isolate);
  Local<Object> error;
  if (CreateSQLiteError(isolate, errstr).ToLocal(&error) &&
      error
          ->Set(isolate->GetCurrentContext(),
                env->errcode_string(),
                Integer::New(isolate, errcode))
          .IsJust()) {
    isolate->ThrowException(error);
  }
}

inline MaybeLocal<Value> NullableSQLiteStringToValue(Isolate* isolate,
                                                     const char* str) {
  if (str == nullptr) {
    return Null(isolate);
  }

  return String::NewFromUtf8(isolate, str, NewStringType::kInternalized)
      .As<Value>();
}

class CustomAggregate {
 public:
  explicit CustomAggregate(Environment* env,
                           Database* db,
                           bool use_bigint_args,
                           Local<Value> start,
                           Local<Function> step_fn,
                           Local<Function> inverse_fn,
                           Local<Function> result_fn)
      : env_(env),
        db_(db),
        use_bigint_args_(use_bigint_args),
        start_(env->isolate(), start),
        step_fn_(env->isolate(), step_fn),
        inverse_fn_(env->isolate(), inverse_fn),
        result_fn_(env->isolate(), result_fn) {}

  static void xStep(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
    xStepBase(ctx, argc, argv, &CustomAggregate::step_fn_);
  }

  static void xInverse(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
    xStepBase(ctx, argc, argv, &CustomAggregate::inverse_fn_);
  }

  static void xFinal(sqlite3_context* ctx) { xValueBase(ctx, true); }

  static void xValue(sqlite3_context* ctx) { xValueBase(ctx, false); }

  static void xDestroy(void* self) {
    delete static_cast<CustomAggregate*>(self);
  }

 private:
  struct aggregate_data {
    Global<Value> value;
    bool initialized;
    bool is_window;
  };

  static inline void xStepBase(sqlite3_context* ctx,
                               int argc,
                               sqlite3_value** argv,
                               Global<Function> CustomAggregate::*mptr) {
    CustomAggregate* self =
        static_cast<CustomAggregate*>(sqlite3_user_data(ctx));
    Environment* env = self->env_;
    Isolate* isolate = env->isolate();
    auto agg = self->GetAggregate(ctx);

    if (!agg) {
      return;
    }

    auto recv = Undefined(isolate);
    LocalVector<Value> js_argv(isolate);
    js_argv.emplace_back(Local<Value>::New(isolate, agg->value));

    for (int i = 0; i < argc; ++i) {
      sqlite3_value* value = argv[i];
      MaybeLocal<Value> js_val;
      SQLITE_VALUE_TO_JS(value, isolate, self->use_bigint_args_, js_val, value);
      if (js_val.IsEmpty()) {
        // Ignore the SQLite error because a JavaScript exception is pending.
        self->db_->SetIgnoreNextSQLiteError(true);
        sqlite3_result_error(ctx, "", 0);
        return;
      }

      Local<Value> local;
      if (!js_val.ToLocal(&local)) {
        // Ignore the SQLite error because a JavaScript exception is pending.
        self->db_->SetIgnoreNextSQLiteError(true);
        sqlite3_result_error(ctx, "", 0);
        return;
      }

      js_argv.emplace_back(local);
    }

    Local<Value> ret;
    if (!(self->*mptr)
             .Get(isolate)
             ->Call(env->context(), recv, argc + 1, js_argv.data())
             .ToLocal(&ret)) {
      self->db_->SetIgnoreNextSQLiteError(true);
      sqlite3_result_error(ctx, "", 0);
      return;
    }

    agg->value.Reset(isolate, ret);
  }

  static inline void xValueBase(sqlite3_context* ctx, bool is_final) {
    CustomAggregate* self =
        static_cast<CustomAggregate*>(sqlite3_user_data(ctx));
    Environment* env = self->env_;
    Isolate* isolate = env->isolate();
    auto agg = self->GetAggregate(ctx);

    if (!agg) {
      return;
    }

    if (!is_final) {
      agg->is_window = true;
    } else if (agg->is_window) {
      DestroyAggregateData(ctx);
      return;
    }

    Local<Value> result;
    if (!self->result_fn_.IsEmpty()) {
      Local<Function> fn =
          Local<Function>::New(env->isolate(), self->result_fn_);
      Local<Value> js_arg[] = {Local<Value>::New(isolate, agg->value)};

      if (!fn->Call(env->context(), Null(isolate), 1, js_arg)
               .ToLocal(&result)) {
        self->db_->SetIgnoreNextSQLiteError(true);
        sqlite3_result_error(ctx, "", 0);
      }
    } else {
      result = Local<Value>::New(isolate, agg->value);
    }

    if (!result.IsEmpty()) {
      JSValueToSQLiteResult(isolate, ctx, result);
    }

    if (is_final) {
      DestroyAggregateData(ctx);
    }
  }

  static void DestroyAggregateData(sqlite3_context* ctx) {
    aggregate_data* agg = static_cast<aggregate_data*>(
        sqlite3_aggregate_context(ctx, sizeof(aggregate_data)));
    CHECK(agg->initialized);
    agg->value.Reset();
  }

  aggregate_data* GetAggregate(sqlite3_context* ctx) {
    aggregate_data* agg = static_cast<aggregate_data*>(
        sqlite3_aggregate_context(ctx, sizeof(aggregate_data)));
    if (!agg->initialized) {
      Isolate* isolate = env_->isolate();
      Local<Value> start_v = Local<Value>::New(isolate, start_);
      if (start_v->IsFunction()) {
        auto fn = start_v.As<Function>();
        MaybeLocal<Value> retval =
            fn->Call(env_->context(), Null(isolate), 0, nullptr);
        if (!retval.ToLocal(&start_v)) {
          db_->SetIgnoreNextSQLiteError(true);
          sqlite3_result_error(ctx, "", 0);
          return nullptr;
        }
      }

      agg->value.Reset(env_->isolate(), start_v);
      agg->initialized = true;
    }

    return agg;
  }

  Environment* env_;
  Database* db_;
  bool use_bigint_args_;
  Global<Value> start_;
  Global<Function> step_fn_;
  Global<Function> inverse_fn_;
  Global<Function> result_fn_;
};

template <typename T>
class SQLiteAsyncTask : public ThreadPoolWork {
 public:
  explicit SQLiteAsyncTask(
      Environment* env,
      Database* db,
      Local<Promise::Resolver> resolver,
      std::function<T()> work,
      std::function<void(T, Local<Promise::Resolver>)> after)
      : ThreadPoolWork(env, "node_sqlite_async_task"),
        env_(env),
        db_(db),
        work_(work),
        after_(after) {
    resolver_.Reset(env->isolate(), resolver);
  }

  void DoThreadPoolWork() override {
    if (work_) {
      result_ = work_();
    }
  }

  void AfterThreadPoolWork(int status) override {
    Isolate* isolate = env_->isolate();
    HandleScope handle_scope(isolate);
    Local<Promise::Resolver> resolver =
        Local<Promise::Resolver>::New(isolate, resolver_);

    if (after_) {
      after_(result_, resolver);
      Finalize();
    }
  }

  void Finalize() { db_->RemoveAsyncTask(this); }

 private:
  Environment* env_;
  Database* db_;
  Global<Promise::Resolver> resolver_;
  std::function<T()> work_ = nullptr;
  std::function<void(T, Local<Promise::Resolver>)> after_ = nullptr;
  T result_;
};

// TODO(geeksilva97): Replace BackupJob usage with SQLiteAsyncTask
class BackupJob : public ThreadPoolWork {
 public:
  explicit BackupJob(Environment* env,
                     Database* source,
                     Local<Promise::Resolver> resolver,
                     std::string source_db,
                     std::string destination_name,
                     std::string dest_db,
                     int pages,
                     Local<Function> progressFunc)
      : ThreadPoolWork(env, "node_sqlite3.BackupJob"),
        env_(env),
        source_(source),
        pages_(pages),
        source_db_(std::move(source_db)),
        destination_name_(std::move(destination_name)),
        dest_db_(std::move(dest_db)) {
    resolver_.Reset(env->isolate(), resolver);
    progressFunc_.Reset(env->isolate(), progressFunc);
  }

  void ScheduleBackup() {
    Isolate* isolate = env()->isolate();
    HandleScope handle_scope(isolate);
    backup_status_ = sqlite3_open_v2(
        destination_name_.c_str(),
        &dest_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI,
        nullptr);
    Local<Promise::Resolver> resolver =
        Local<Promise::Resolver>::New(env()->isolate(), resolver_);
    if (backup_status_ != SQLITE_OK) {
      HandleBackupError(resolver);
      return;
    }

    backup_ = sqlite3_backup_init(
        dest_, dest_db_.c_str(), source_->Connection(), source_db_.c_str());
    if (backup_ == nullptr) {
      HandleBackupError(resolver);
      return;
    }

    this->ScheduleWork();
  }

  void DoThreadPoolWork() override {
    backup_status_ = sqlite3_backup_step(backup_, pages_);
  }

  void AfterThreadPoolWork(int status) override {
    HandleScope handle_scope(env()->isolate());
    Local<Promise::Resolver> resolver =
        Local<Promise::Resolver>::New(env()->isolate(), resolver_);

    if (!(backup_status_ == SQLITE_OK || backup_status_ == SQLITE_DONE ||
          backup_status_ == SQLITE_BUSY || backup_status_ == SQLITE_LOCKED)) {
      HandleBackupError(resolver, backup_status_);
      return;
    }

    int total_pages = sqlite3_backup_pagecount(backup_);
    int remaining_pages = sqlite3_backup_remaining(backup_);
    if (remaining_pages != 0) {
      Local<Function> fn =
          Local<Function>::New(env()->isolate(), progressFunc_);
      if (!fn.IsEmpty()) {
        Local<Object> progress_info = Object::New(env()->isolate());
        if (progress_info
                ->Set(env()->context(),
                      env()->total_pages_string(),
                      Integer::New(env()->isolate(), total_pages))
                .IsNothing() ||
            progress_info
                ->Set(env()->context(),
                      env()->remaining_pages_string(),
                      Integer::New(env()->isolate(), remaining_pages))
                .IsNothing()) {
          return;
        }

        Local<Value> argv[] = {progress_info};
        TryCatch try_catch(env()->isolate());
        USE(fn->Call(env()->context(), Null(env()->isolate()), 1, argv));
        if (try_catch.HasCaught()) {
          Finalize();
          resolver->Reject(env()->context(), try_catch.Exception()).ToChecked();
          return;
        }
      }

      // There's still work to do
      this->ScheduleWork();
      return;
    }

    if (backup_status_ != SQLITE_DONE) {
      HandleBackupError(resolver);
      return;
    }

    Finalize();
    resolver
        ->Resolve(env()->context(), Integer::New(env()->isolate(), total_pages))
        .ToChecked();
  }

  void Finalize() {
    Cleanup();
    source_->RemoveBackup(this);
  }

  void Cleanup() {
    if (backup_) {
      sqlite3_backup_finish(backup_);
      backup_ = nullptr;
    }

    if (dest_) {
      backup_status_ = sqlite3_errcode(dest_);
      sqlite3_close_v2(dest_);
      dest_ = nullptr;
    }
  }

 private:
  void HandleBackupError(Local<Promise::Resolver> resolver) {
    Local<Object> e;
    if (!CreateSQLiteError(env()->isolate(), dest_).ToLocal(&e)) {
      Finalize();
      return;
    }

    Finalize();
    resolver->Reject(env()->context(), e).ToChecked();
  }

  void HandleBackupError(Local<Promise::Resolver> resolver, int errcode) {
    Local<Object> e;
    if (!CreateSQLiteError(env()->isolate(), errcode).ToLocal(&e)) {
      Finalize();
      return;
    }

    Finalize();
    resolver->Reject(env()->context(), e).ToChecked();
  }

  Environment* env() const { return env_; }

  Environment* env_;
  Database* source_;
  Global<Promise::Resolver> resolver_;
  Global<Function> progressFunc_;
  sqlite3* dest_ = nullptr;
  sqlite3_backup* backup_ = nullptr;
  int pages_;
  int backup_status_ = SQLITE_OK;
  std::string source_db_;
  std::string destination_name_;
  std::string dest_db_;
};

UserDefinedFunction::UserDefinedFunction(Environment* env,
                                         Local<Function> fn,
                                         Database* db,
                                         bool use_bigint_args)
    : env_(env),
      fn_(env->isolate(), fn),
      db_(db),
      use_bigint_args_(use_bigint_args) {}

UserDefinedFunction::~UserDefinedFunction() {}

void UserDefinedFunction::xFunc(sqlite3_context* ctx,
                                int argc,
                                sqlite3_value** argv) {
  UserDefinedFunction* self =
      static_cast<UserDefinedFunction*>(sqlite3_user_data(ctx));
  Environment* env = self->env_;
  Isolate* isolate = env->isolate();
  auto recv = Undefined(isolate);
  auto fn = self->fn_.Get(isolate);
  LocalVector<Value> js_argv(isolate);

  for (int i = 0; i < argc; ++i) {
    sqlite3_value* value = argv[i];
    MaybeLocal<Value> js_val = MaybeLocal<Value>();
    SQLITE_VALUE_TO_JS(value, isolate, self->use_bigint_args_, js_val, value);
    if (js_val.IsEmpty()) {
      // Ignore the SQLite error because a JavaScript exception is pending.
      self->db_->SetIgnoreNextSQLiteError(true);
      sqlite3_result_error(ctx, "", 0);
      return;
    }

    Local<Value> local;
    if (!js_val.ToLocal(&local)) {
      // Ignore the SQLite error because a JavaScript exception is pending.
      self->db_->SetIgnoreNextSQLiteError(true);
      sqlite3_result_error(ctx, "", 0);
      return;
    }

    js_argv.emplace_back(local);
  }

  MaybeLocal<Value> retval =
      fn->Call(env->context(), recv, argc, js_argv.data());
  Local<Value> result;
  if (!retval.ToLocal(&result)) {
    // Ignore the SQLite error because a JavaScript exception is pending.
    self->db_->SetIgnoreNextSQLiteError(true);
    sqlite3_result_error(ctx, "", 0);
    return;
  }

  JSValueToSQLiteResult(isolate, ctx, result);
}

void UserDefinedFunction::xDestroy(void* self) {
  delete static_cast<UserDefinedFunction*>(self);
}

Database::Database(Environment* env,
                   Local<Object> object,
                   DatabaseOpenConfiguration&& open_config,
                   bool open,
                   bool allow_load_extension)
    : BaseObject(env, object), open_config_(std::move(open_config)) {
  MakeWeak();
  connection_ = nullptr;
  allow_load_extension_ = allow_load_extension;
  enable_load_extension_ = allow_load_extension;
  ignore_next_sqlite_error_ = false;

  if (open) {
    Open();
  }
}

void Database::AddBackup(BackupJob* job) {
  backups_.insert(job);
}

void Database::RemoveBackup(BackupJob* job) {
  backups_.erase(job);
}

void Database::AddAsyncTask(ThreadPoolWork* async_task) {
  async_tasks_.insert(async_task);
}

void Database::RemoveAsyncTask(ThreadPoolWork* async_task) {
  async_tasks_.erase(async_task);
}

void Database::DeleteSessions() {
  // all attached sessions need to be deleted before the database is closed
  // https://www.sqlite.org/session/sqlite3session_create.html
  for (auto* session : sessions_) {
    sqlite3session_delete(session);
  }
  sessions_.clear();
}

Database::~Database() {
  FinalizeBackups();

  if (IsOpen()) {
    FinalizeStatements();
    DeleteSessions();
    sqlite3_close_v2(connection_);
    connection_ = nullptr;
  }
}

void Database::MemoryInfo(MemoryTracker* tracker) const {
  // TODO(tniessen): more accurately track the size of all fields
  tracker->TrackFieldWithSize(
      "open_config", sizeof(open_config_), "DatabaseOpenConfiguration");
}

bool Database::Open() {
  if (IsOpen()) {
    THROW_ERR_INVALID_STATE(env(), "database is already open");
    return false;
  }

  // TODO(cjihrig): Support additional flags.
  int default_flags = SQLITE_OPEN_URI;
  int flags = open_config_.get_read_only()
                  ? SQLITE_OPEN_READONLY
                  : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
  int r = sqlite3_open_v2(open_config_.location().c_str(),
                          &connection_,
                          flags | default_flags,
                          nullptr);
  CHECK_ERROR_OR_THROW(env()->isolate(), this, r, SQLITE_OK, false);

  r = sqlite3_db_config(connection_,
                        SQLITE_DBCONFIG_DQS_DML,
                        static_cast<int>(open_config_.get_enable_dqs()),
                        nullptr);
  CHECK_ERROR_OR_THROW(env()->isolate(), this, r, SQLITE_OK, false);
  r = sqlite3_db_config(connection_,
                        SQLITE_DBCONFIG_DQS_DDL,
                        static_cast<int>(open_config_.get_enable_dqs()),
                        nullptr);
  CHECK_ERROR_OR_THROW(env()->isolate(), this, r, SQLITE_OK, false);

  int foreign_keys_enabled;
  r = sqlite3_db_config(
      connection_,
      SQLITE_DBCONFIG_ENABLE_FKEY,
      static_cast<int>(open_config_.get_enable_foreign_keys()),
      &foreign_keys_enabled);
  CHECK_ERROR_OR_THROW(env()->isolate(), this, r, SQLITE_OK, false);
  CHECK_EQ(foreign_keys_enabled, open_config_.get_enable_foreign_keys());

  int defensive_enabled;
  r = sqlite3_db_config(connection_,
                        SQLITE_DBCONFIG_DEFENSIVE,
                        static_cast<int>(open_config_.get_enable_defensive()),
                        &defensive_enabled);
  CHECK_ERROR_OR_THROW(env()->isolate(), this, r, SQLITE_OK, false);
  CHECK_EQ(defensive_enabled, open_config_.get_enable_defensive());

  sqlite3_busy_timeout(connection_, open_config_.get_timeout());

  if (allow_load_extension_) {
    if (env()->permission()->enabled()) [[unlikely]] {
      THROW_ERR_LOAD_SQLITE_EXTENSION(env(),
                                      "Cannot load SQLite extensions when the "
                                      "permission model is enabled.");
      return false;
    }
    const int load_extension_ret = sqlite3_db_config(
        connection_, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 1, nullptr);
    CHECK_ERROR_OR_THROW(
        env()->isolate(), this, load_extension_ret, SQLITE_OK, false);
  }

  return true;
}

void Database::FinalizeBackups() {
  for (auto backup : backups_) {
    backup->Cleanup();
  }

  backups_.clear();
}

void Database::FinalizeStatements() {
  for (auto stmt : statements_) {
    stmt->Finalize();
  }

  statements_.clear();
}

void Database::UntrackStatement(Statement* statement) {
  auto it = statements_.find(statement);
  if (it != statements_.end()) {
    statements_.erase(it);
  }
}

inline bool Database::IsOpen() {
  return connection_ != nullptr;
}

inline sqlite3* Database::Connection() {
  return connection_;
}

void Database::SetIgnoreNextSQLiteError(bool ignore) {
  ignore_next_sqlite_error_ = ignore;
}

bool Database::ShouldIgnoreSQLiteError() {
  return ignore_next_sqlite_error_;
}

void Database::CreateTagStore(const FunctionCallbackInfo<Value>& args) {
  Database* db = BaseObject::Unwrap<Database>(args.This());
  Environment* env = Environment::GetCurrent(args);

  if (!db->IsOpen()) {
    THROW_ERR_INVALID_STATE(env, "database is not open");
    return;
  }
  int capacity = 1000;
  if (args.Length() > 0 && args[0]->IsNumber()) {
    capacity = args[0].As<Number>()->Value();
  }

  BaseObjectPtr<SQLTagStore> session =
      SQLTagStore::Create(env, BaseObjectWeakPtr<Database>(db), capacity);
  if (!session) {
    // Handle error if creation failed
    THROW_ERR_SQLITE_ERROR(env->isolate(), "Failed to create SQLTagStore");
    return;
  }
  args.GetReturnValue().Set(session->object());
}

std::optional<std::string> ValidateDatabasePath(Environment* env,
                                                Local<Value> path,
                                                const std::string& field_name) {
  constexpr auto has_null_bytes = [](std::string_view str) {
    return str.find('\0') != std::string_view::npos;
  };
  if (path->IsString()) {
    Utf8Value location(env->isolate(), path.As<String>());
    if (!has_null_bytes(location.ToStringView())) {
      return location.ToString();
    }
  } else if (path->IsUint8Array()) {
    Local<Uint8Array> buffer = path.As<Uint8Array>();
    size_t byteOffset = buffer->ByteOffset();
    size_t byteLength = buffer->ByteLength();
    auto data =
        static_cast<const uint8_t*>(buffer->Buffer()->Data()) + byteOffset;
    if (std::find(data, data + byteLength, 0) == data + byteLength) {
      return std::string(reinterpret_cast<const char*>(data), byteLength);
    }
  } else if (path->IsObject()) {  // When is URL
    auto url = path.As<Object>();
    Local<Value> href;
    if (url->Get(env->context(), env->href_string()).ToLocal(&href) &&
        href->IsString()) {
      Utf8Value location_value(env->isolate(), href.As<String>());
      auto location = location_value.ToStringView();
      if (!has_null_bytes(location)) {
        CHECK(ada::can_parse(location));
        if (!location.starts_with("file:")) {
          THROW_ERR_INVALID_URL_SCHEME(env->isolate());
          return std::nullopt;
        }

        return location_value.ToString();
      }
    }
  }

  THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                             "The \"%s\" argument must be a string, "
                             "Uint8Array, or URL without null bytes.",
                             field_name.c_str());

  return std::nullopt;
}

inline void DatabaseNew(const FunctionCallbackInfo<Value>& args,
                        bool async = true) {
  Environment* env = Environment::GetCurrent(args);
  if (!args.IsConstructCall()) {
    THROW_ERR_CONSTRUCT_CALL_REQUIRED(env);
    return;
  }

  std::optional<std::string> location =
      ValidateDatabasePath(env, args[0], "path");
  if (!location.has_value()) {
    return;
  }

  DatabaseOpenConfiguration open_config(std::move(location.value()));
  bool open = true;
  bool allow_load_extension = false;
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

    Local<String> allow_extension_string =
        FIXED_ONE_BYTE_STRING(env->isolate(), "allowExtension");
    Local<Value> allow_extension_v;
    if (!options->Get(env->context(), allow_extension_string)
             .ToLocal(&allow_extension_v)) {
      return;
    }

    if (!allow_extension_v->IsUndefined()) {
      if (!allow_extension_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.allowExtension\" argument must be a boolean.");
        return;
      }
      allow_load_extension = allow_extension_v.As<Boolean>()->Value();
    }

    Local<Value> timeout_v;
    if (!options->Get(env->context(), env->timeout_string())
             .ToLocal(&timeout_v)) {
      return;
    }

    if (!timeout_v->IsUndefined()) {
      if (!timeout_v->IsInt32()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.timeout\" argument must be an integer.");
        return;
      }

      open_config.set_timeout(timeout_v.As<Int32>()->Value());
    }

    Local<Value> read_bigints_v;
    if (options->Get(env->context(), env->read_bigints_string())
            .ToLocal(&read_bigints_v)) {
      if (!read_bigints_v->IsUndefined()) {
        if (!read_bigints_v->IsBoolean()) {
          THROW_ERR_INVALID_ARG_TYPE(
              env->isolate(),
              R"(The "options.readBigInts" argument must be a boolean.)");
          return;
        }
        open_config.set_use_big_ints(read_bigints_v.As<Boolean>()->Value());
      }
    }

    Local<Value> return_arrays_v;
    if (options->Get(env->context(), env->return_arrays_string())
            .ToLocal(&return_arrays_v)) {
      if (!return_arrays_v->IsUndefined()) {
        if (!return_arrays_v->IsBoolean()) {
          THROW_ERR_INVALID_ARG_TYPE(
              env->isolate(),
              R"(The "options.returnArrays" argument must be a boolean.)");
          return;
        }
        open_config.set_return_arrays(return_arrays_v.As<Boolean>()->Value());
      }
    }

    Local<Value> allow_bare_named_params_v;
    if (options->Get(env->context(), env->allow_bare_named_params_string())
            .ToLocal(&allow_bare_named_params_v)) {
      if (!allow_bare_named_params_v->IsUndefined()) {
        if (!allow_bare_named_params_v->IsBoolean()) {
          THROW_ERR_INVALID_ARG_TYPE(
              env->isolate(),
              R"(The "options.allowBareNamedParameters" )"
              "argument must be a boolean.");
          return;
        }
        open_config.set_allow_bare_named_params(
            allow_bare_named_params_v.As<Boolean>()->Value());
      }
    }

    Local<Value> allow_unknown_named_params_v;
    if (options->Get(env->context(), env->allow_unknown_named_params_string())
            .ToLocal(&allow_unknown_named_params_v)) {
      if (!allow_unknown_named_params_v->IsUndefined()) {
        if (!allow_unknown_named_params_v->IsBoolean()) {
          THROW_ERR_INVALID_ARG_TYPE(
              env->isolate(),
              R"(The "options.allowUnknownNamedParameters" )"
              "argument must be a boolean.");
          return;
        }
        open_config.set_allow_unknown_named_params(
            allow_unknown_named_params_v.As<Boolean>()->Value());
      }
    }

    Local<Value> defensive_v;
    if (!options->Get(env->context(), env->defensive_string())
             .ToLocal(&defensive_v)) {
      return;
    }
    if (!defensive_v->IsUndefined()) {
      if (!defensive_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.defensive\" argument must be a boolean.");
        return;
      }
      open_config.set_enable_defensive(defensive_v.As<Boolean>()->Value());
    }
  }

  open_config.set_async(async);
  new Database(
      env, args.This(), std::move(open_config), open, allow_load_extension);
}

void Database::New(const FunctionCallbackInfo<Value>& args) {
  DatabaseNew(args, false);
}

void Database::NewAsync(const FunctionCallbackInfo<Value>& args) {
  DatabaseNew(args, true);
}

void Database::Open(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  db->Open();
}

void Database::IsOpenGetter(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  args.GetReturnValue().Set(db->IsOpen());
}

void Database::IsTransactionGetter(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");
  args.GetReturnValue().Set(sqlite3_get_autocommit(db->connection_) == 0);
}

void Database::Close(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");
  db->FinalizeStatements();
  db->DeleteSessions();
  int r = sqlite3_close_v2(db->connection_);
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());
  db->connection_ = nullptr;
}

void Database::Dispose(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch(args.GetIsolate());
  Close(args);
  if (try_catch.HasCaught()) {
    CHECK(try_catch.CanContinue());
  }
}

void Database::Prepare(const FunctionCallbackInfo<Value>& args) {
  Database* db;
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

  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());
  BaseObjectPtr<Statement> stmt =
      Statement::Create(env, BaseObjectPtr<Database>(db), s);
  db->statements_.insert(stmt.get());
  args.GetReturnValue().Set(stmt->object());
}

template <typename T>
Local<Promise::Resolver> MakeSQLiteAsyncWork(
    Environment* env,
    Database* db,
    std::function<T()> task,
    std::function<void(T, Local<Promise::Resolver>)> after) {
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(env->context()).ToLocal(&resolver)) {
    return Local<Promise::Resolver>();
  }

  auto* work = new SQLiteAsyncTask<T>(env, db, resolver, task, after);
  work->ScheduleWork();
  db->AddAsyncTask(work);
  return resolver;
}

void Database::Exec(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");

  if (!args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"sql\" argument must be a string.");
    return;
  }

  auto sql = Utf8Value(env->isolate(), args[0].As<String>()).ToString();
  auto task = [sql, db]() -> int {
    return sqlite3_exec(
        db->connection_, sql.c_str(), nullptr, nullptr, nullptr);
  };

  if (!db->open_config_.get_async()) {
    int r = task();
    CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());
    return;
  }

  auto after = [db, env, isolate](int exec_result,
                                  Local<Promise::Resolver> resolver) {
    if (exec_result != SQLITE_OK) {
      if (db->ShouldIgnoreSQLiteError()) {
        db->SetIgnoreNextSQLiteError(false);
        return;
      }

      Local<Object> e;
      if (!CreateSQLiteError(isolate, db->Connection()).ToLocal(&e)) {
        return;
      }

      resolver->Reject(env->context(), e).FromJust();
      return;
    }

    resolver->Resolve(env->context(), Undefined(env->isolate())).FromJust();
  };

  Local<Promise::Resolver> resolver =
      MakeSQLiteAsyncWork<int>(env, db, task, after);
  if (resolver.IsEmpty()) {
    return;
  }

  args.GetReturnValue().Set(resolver->GetPromise());
}

void Database::CustomFunction(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");

  if (!args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"name\" argument must be a string.");
    return;
  }

  int fn_index = args.Length() < 3 ? 1 : 2;
  bool use_bigint_args = false;
  bool varargs = false;
  bool deterministic = false;
  bool direct_only = false;

  if (fn_index > 1) {
    if (!args[1]->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                                 "The \"options\" argument must be an object.");
      return;
    }

    Local<Object> options = args[1].As<Object>();
    Local<Value> use_bigint_args_v;
    if (!options
             ->Get(env->context(),
                   FIXED_ONE_BYTE_STRING(env->isolate(), "useBigIntArguments"))
             .ToLocal(&use_bigint_args_v)) {
      return;
    }

    if (!use_bigint_args_v->IsUndefined()) {
      if (!use_bigint_args_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.useBigIntArguments\" argument must be a boolean.");
        return;
      }
      use_bigint_args = use_bigint_args_v.As<Boolean>()->Value();
    }

    Local<Value> varargs_v;
    if (!options
             ->Get(env->context(),
                   FIXED_ONE_BYTE_STRING(env->isolate(), "varargs"))
             .ToLocal(&varargs_v)) {
      return;
    }

    if (!varargs_v->IsUndefined()) {
      if (!varargs_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.varargs\" argument must be a boolean.");
        return;
      }
      varargs = varargs_v.As<Boolean>()->Value();
    }

    Local<Value> deterministic_v;
    if (!options
             ->Get(env->context(),
                   FIXED_ONE_BYTE_STRING(env->isolate(), "deterministic"))
             .ToLocal(&deterministic_v)) {
      return;
    }

    if (!deterministic_v->IsUndefined()) {
      if (!deterministic_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.deterministic\" argument must be a boolean.");
        return;
      }
      deterministic = deterministic_v.As<Boolean>()->Value();
    }

    Local<Value> direct_only_v;
    if (!options
             ->Get(env->context(),
                   FIXED_ONE_BYTE_STRING(env->isolate(), "directOnly"))
             .ToLocal(&direct_only_v)) {
      return;
    }

    if (!direct_only_v->IsUndefined()) {
      if (!direct_only_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.directOnly\" argument must be a boolean.");
        return;
      }
      direct_only = direct_only_v.As<Boolean>()->Value();
    }
  }

  if (!args[fn_index]->IsFunction()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"function\" argument must be a function.");
    return;
  }

  Utf8Value name(env->isolate(), args[0].As<String>());
  Local<Function> fn = args[fn_index].As<Function>();

  int argc = 0;
  if (varargs) {
    argc = -1;
  } else {
    Local<Value> js_len;
    if (!fn->Get(env->context(), env->length_string()).ToLocal(&js_len)) {
      return;
    }
    argc = js_len.As<Int32>()->Value();
  }

  UserDefinedFunction* user_data =
      new UserDefinedFunction(env, fn, db, use_bigint_args);
  int text_rep = SQLITE_UTF8;

  if (deterministic) {
    text_rep |= SQLITE_DETERMINISTIC;
  }

  if (direct_only) {
    text_rep |= SQLITE_DIRECTONLY;
  }

  int r = sqlite3_create_function_v2(db->connection_,
                                     *name,
                                     argc,
                                     text_rep,
                                     user_data,
                                     UserDefinedFunction::xFunc,
                                     nullptr,
                                     nullptr,
                                     UserDefinedFunction::xDestroy);
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());
}

void Database::Location(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");

  std::string db_name = "main";
  if (!args[0]->IsUndefined()) {
    if (!args[0]->IsString()) {
      THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                                 "The \"dbName\" argument must be a string.");
      return;
    }

    db_name = Utf8Value(env->isolate(), args[0].As<String>()).ToString();
  }

  const char* db_filename =
      sqlite3_db_filename(db->connection_, db_name.c_str());
  if (!db_filename || db_filename[0] == '\0') {
    args.GetReturnValue().Set(Null(env->isolate()));
    return;
  }

  Local<String> ret;
  if (String::NewFromUtf8(env->isolate(), db_filename).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void Database::AggregateFunction(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");
  Utf8Value name(env->isolate(), args[0].As<String>());
  Local<Object> options = args[1].As<Object>();
  Local<Value> start_v;
  if (!options->Get(env->context(), env->start_string()).ToLocal(&start_v)) {
    return;
  }

  if (start_v->IsUndefined()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"options.start\" argument must be a "
                               "function or a primitive value.");
    return;
  }

  Local<Value> step_v;
  if (!options->Get(env->context(), env->step_string()).ToLocal(&step_v)) {
    return;
  }

  if (!step_v->IsFunction()) {
    THROW_ERR_INVALID_ARG_TYPE(
        env->isolate(), "The \"options.step\" argument must be a function.");
    return;
  }

  Local<Value> result_v;
  if (!options->Get(env->context(), env->result_string()).ToLocal(&result_v)) {
    return;
  }

  bool use_bigint_args = false;
  bool varargs = false;
  bool direct_only = false;
  Local<Value> use_bigint_args_v;
  Local<Function> inverseFunc = Local<Function>();
  if (!options
           ->Get(env->context(),
                 FIXED_ONE_BYTE_STRING(env->isolate(), "useBigIntArguments"))
           .ToLocal(&use_bigint_args_v)) {
    return;
  }

  if (!use_bigint_args_v->IsUndefined()) {
    if (!use_bigint_args_v->IsBoolean()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(),
          "The \"options.useBigIntArguments\" argument must be a boolean.");
      return;
    }
    use_bigint_args = use_bigint_args_v.As<Boolean>()->Value();
  }

  Local<Value> varargs_v;
  if (!options
           ->Get(env->context(),
                 FIXED_ONE_BYTE_STRING(env->isolate(), "varargs"))
           .ToLocal(&varargs_v)) {
    return;
  }

  if (!varargs_v->IsUndefined()) {
    if (!varargs_v->IsBoolean()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(),
          "The \"options.varargs\" argument must be a boolean.");
      return;
    }
    varargs = varargs_v.As<Boolean>()->Value();
  }

  Local<Value> direct_only_v;
  if (!options
           ->Get(env->context(),
                 FIXED_ONE_BYTE_STRING(env->isolate(), "directOnly"))
           .ToLocal(&direct_only_v)) {
    return;
  }

  if (!direct_only_v->IsUndefined()) {
    if (!direct_only_v->IsBoolean()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(),
          "The \"options.directOnly\" argument must be a boolean.");
      return;
    }
    direct_only = direct_only_v.As<Boolean>()->Value();
  }

  Local<Value> inverse_v;
  if (!options->Get(env->context(), env->inverse_string())
           .ToLocal(&inverse_v)) {
    return;
  }

  if (!inverse_v->IsUndefined()) {
    if (!inverse_v->IsFunction()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(),
          "The \"options.inverse\" argument must be a function.");
      return;
    }
    inverseFunc = inverse_v.As<Function>();
  }

  Local<Function> stepFunction = step_v.As<Function>();
  Local<Function> resultFunction =
      result_v->IsFunction() ? result_v.As<Function>() : Local<Function>();
  int argc = -1;
  if (!varargs) {
    Local<Value> js_len;
    if (!stepFunction->Get(env->context(), env->length_string())
             .ToLocal(&js_len)) {
      return;
    }

    // Subtract 1 because the first argument is the aggregate value.
    argc = js_len.As<Int32>()->Value() - 1;
    if (!inverseFunc.IsEmpty() &&
        !inverseFunc->Get(env->context(), env->length_string())
             .ToLocal(&js_len)) {
      return;
    }

    argc = std::max({argc, js_len.As<Int32>()->Value() - 1, 0});
  }

  int text_rep = SQLITE_UTF8;
  if (direct_only) {
    text_rep |= SQLITE_DIRECTONLY;
  }

  auto xInverse = !inverseFunc.IsEmpty() ? CustomAggregate::xInverse : nullptr;
  auto xValue = xInverse ? CustomAggregate::xValue : nullptr;
  int r = sqlite3_create_window_function(db->connection_,
                                         *name,
                                         argc,
                                         text_rep,
                                         new CustomAggregate(env,
                                                             db,
                                                             use_bigint_args,
                                                             start_v,
                                                             stepFunction,
                                                             inverseFunc,
                                                             resultFunction),
                                         CustomAggregate::xStep,
                                         CustomAggregate::xFinal,
                                         xValue,
                                         xInverse,
                                         CustomAggregate::xDestroy);
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());
}

void Database::CreateSession(const FunctionCallbackInfo<Value>& args) {
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

    Local<String> table_key = env->table_string();
    bool hasIt;
    if (!options->HasOwnProperty(env->context(), table_key).To(&hasIt)) {
      return;
    }
    if (hasIt) {
      Local<Value> table_value;
      if (!options->Get(env->context(), table_key).ToLocal(&table_value)) {
        return;
      }

      if (table_value->IsString()) {
        table = Utf8Value(env->isolate(), table_value).ToString();
      } else {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(), "The \"options.table\" argument must be a string.");
        return;
      }
    }

    Local<String> db_key = FIXED_ONE_BYTE_STRING(env->isolate(), "db");

    if (!options->HasOwnProperty(env->context(), db_key).To(&hasIt)) {
      return;
    }
    if (hasIt) {
      Local<Value> db_value;
      if (!options->Get(env->context(), db_key).ToLocal(&db_value)) {
        // An error will have been scheduled.
        return;
      }
      if (db_value->IsString()) {
        db_name = Utf8Value(env->isolate(), db_value).ToString();
      } else {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(), "The \"options.db\" argument must be a string.");
        return;
      }
    }
  }

  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");

  sqlite3_session* pSession;
  int r = sqlite3session_create(db->connection_, db_name.c_str(), &pSession);
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());
  db->sessions_.insert(pSession);

  r = sqlite3session_attach(pSession, table == "" ? nullptr : table.c_str());
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());

  BaseObjectPtr<Session> session =
      Session::Create(env, BaseObjectWeakPtr<Database>(db), pSession);
  args.GetReturnValue().Set(session->object());
}

void Backup(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1 || !args[0]->IsObject()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"sourceDb\" argument must be an object.");
    return;
  }

  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args[0].As<Object>());
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");
  std::optional<std::string> dest_path =
      ValidateDatabasePath(env, args[1], "path");
  if (!dest_path.has_value()) {
    return;
  }

  int rate = 100;
  std::string source_db = "main";
  std::string dest_db = "main";
  Local<Function> progressFunc = Local<Function>();

  if (args.Length() > 2) {
    if (!args[2]->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                                 "The \"options\" argument must be an object.");
      return;
    }

    Local<Object> options = args[2].As<Object>();
    Local<Value> rate_v;
    if (!options->Get(env->context(), env->rate_string()).ToLocal(&rate_v)) {
      return;
    }

    if (!rate_v->IsUndefined()) {
      if (!rate_v->IsInt32()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.rate\" argument must be an integer.");
        return;
      }
      rate = rate_v.As<Int32>()->Value();
    }

    Local<Value> source_v;
    if (!options->Get(env->context(), env->source_string())
             .ToLocal(&source_v)) {
      return;
    }

    if (!source_v->IsUndefined()) {
      if (!source_v->IsString()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.source\" argument must be a string.");
        return;
      }

      source_db = Utf8Value(env->isolate(), source_v.As<String>()).ToString();
    }

    Local<Value> target_v;
    if (!options->Get(env->context(), env->target_string())
             .ToLocal(&target_v)) {
      return;
    }

    if (!target_v->IsUndefined()) {
      if (!target_v->IsString()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.target\" argument must be a string.");
        return;
      }

      dest_db = Utf8Value(env->isolate(), target_v.As<String>()).ToString();
    }

    Local<Value> progress_v;
    if (!options->Get(env->context(), env->progress_string())
             .ToLocal(&progress_v)) {
      return;
    }

    if (!progress_v->IsUndefined()) {
      if (!progress_v->IsFunction()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.progress\" argument must be a function.");
        return;
      }
      progressFunc = progress_v.As<Function>();
    }
  }

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(env->context()).ToLocal(&resolver)) {
    return;
  }

  args.GetReturnValue().Set(resolver->GetPromise());
  BackupJob* job = new BackupJob(env,
                                 db,
                                 resolver,
                                 std::move(source_db),
                                 dest_path.value(),
                                 std::move(dest_db),
                                 rate,
                                 progressFunc);
  db->AddBackup(job);
  job->ScheduleBackup();
}

struct ConflictCallbackContext {
  std::function<bool(std::string_view)> filterCallback;
  std::function<int(int)> conflictCallback;
};

// the reason for using static functions here is that SQLite needs a
// function pointer

static int xConflict(void* pCtx, int eConflict, sqlite3_changeset_iter* pIter) {
  auto ctx = static_cast<ConflictCallbackContext*>(pCtx);
  if (!ctx->conflictCallback) return SQLITE_CHANGESET_ABORT;
  return ctx->conflictCallback(eConflict);
}

static int xFilter(void* pCtx, const char* zTab) {
  auto ctx = static_cast<ConflictCallbackContext*>(pCtx);
  if (!ctx->filterCallback) return 1;
  return ctx->filterCallback(zTab) ? 1 : 0;
}

void Database::ApplyChangeset(const FunctionCallbackInfo<Value>& args) {
  ConflictCallbackContext context;

  Database* db;
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
    Local<Value> conflictValue;
    if (!options->Get(env->context(), env->onconflict_string())
             .ToLocal(&conflictValue)) {
      // An error will have been scheduled.
      return;
    }

    if (!conflictValue->IsUndefined()) {
      if (!conflictValue->IsFunction()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.onConflict\" argument must be a function.");
        return;
      }
      Local<Function> conflictFunc = conflictValue.As<Function>();
      context.conflictCallback = [env, conflictFunc](int conflictType) -> int {
        Local<Value> argv[] = {Integer::New(env->isolate(), conflictType)};
        TryCatch try_catch(env->isolate());
        Local<Value> result =
            conflictFunc->Call(env->context(), Null(env->isolate()), 1, argv)
                .FromMaybe(Local<Value>());
        if (try_catch.HasCaught()) {
          try_catch.ReThrow();
          return SQLITE_CHANGESET_ABORT;
        }
        constexpr auto invalid_value = -1;
        if (!result->IsInt32()) return invalid_value;
        return result->Int32Value(env->context()).FromJust();
      };
    }

    bool hasIt;
    if (!options->HasOwnProperty(env->context(), env->filter_string())
             .To(&hasIt)) {
      return;
    }
    if (hasIt) {
      Local<Value> filterValue;
      if (!options->Get(env->context(), env->filter_string())
               .ToLocal(&filterValue)) {
        // An error will have been scheduled.
        return;
      }

      if (!filterValue->IsFunction()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.filter\" argument must be a function.");
        return;
      }

      Local<Function> filterFunc = filterValue.As<Function>();

      context.filterCallback = [&](std::string_view item) -> bool {
        // If there was an error in the previous call to the filter's
        // callback, we skip calling it again.
        if (db->ignore_next_sqlite_error_) {
          return false;
        }

        Local<Value> argv[1];
        if (!ToV8Value(env->context(), item, env->isolate())
                 .ToLocal(&argv[0])) {
          db->SetIgnoreNextSQLiteError(true);
          return false;
        }

        Local<Value> result;
        if (!filterFunc->Call(env->context(), Null(env->isolate()), 1, argv)
                 .ToLocal(&result)) {
          db->SetIgnoreNextSQLiteError(true);
          return false;
        }

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
      static_cast<void*>(&context));
  if (r == SQLITE_OK) {
    args.GetReturnValue().Set(true);
    return;
  }
  if (r == SQLITE_ABORT) {
    // this is not an error, return false
    args.GetReturnValue().Set(false);
    return;
  }
  THROW_ERR_SQLITE_ERROR(env->isolate(), r);
}

void Database::EnableLoadExtension(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  auto isolate = args.GetIsolate();
  if (!args[0]->IsBoolean()) {
    THROW_ERR_INVALID_ARG_TYPE(isolate,
                               "The \"allow\" argument must be a boolean.");
    return;
  }

  const int enable = args[0].As<Boolean>()->Value();

  if (db->allow_load_extension_ == false && enable == true) {
    THROW_ERR_INVALID_STATE(
        isolate,
        "Cannot enable extension loading because it was disabled at database "
        "creation.");
    return;
  }
  db->enable_load_extension_ = enable;
  const int load_extension_ret = sqlite3_db_config(
      db->connection_, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, enable, nullptr);
  CHECK_ERROR_OR_THROW(isolate, db, load_extension_ret, SQLITE_OK, void());
}

void Database::EnableDefensive(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");

  auto isolate = args.GetIsolate();
  if (!args[0]->IsBoolean()) {
    THROW_ERR_INVALID_ARG_TYPE(isolate,
                               "The \"active\" argument must be a boolean.");
    return;
  }

  const int enable = args[0].As<Boolean>()->Value();
  int defensive_enabled;
  const int defensive_ret = sqlite3_db_config(
      db->connection_, SQLITE_DBCONFIG_DEFENSIVE, enable, &defensive_enabled);
  CHECK_ERROR_OR_THROW(isolate, db, defensive_ret, SQLITE_OK, void());
}

void Database::LoadExtension(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, db->connection_ == nullptr, "database is not open");
  THROW_AND_RETURN_ON_BAD_STATE(
      env, !db->allow_load_extension_, "extension loading is not allowed");
  THROW_AND_RETURN_ON_BAD_STATE(
      env, !db->enable_load_extension_, "extension loading is not allowed");

  if (!args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"path\" argument must be a string.");
    return;
  }

  auto isolate = env->isolate();

  BufferValue path(isolate, args[0]);
  BufferValue entryPoint(isolate, args[1]);
  CHECK_NOT_NULL(*path);
  ToNamespacedPath(env, &path);
  if (*entryPoint == nullptr) {
    ToNamespacedPath(env, &entryPoint);
  }
  THROW_IF_INSUFFICIENT_PERMISSIONS(
      env, permission::PermissionScope::kFileSystemRead, path.ToStringView());
  char* errmsg = nullptr;
  const int r =
      sqlite3_load_extension(db->connection_, *path, *entryPoint, &errmsg);
  if (r != SQLITE_OK) {
    isolate->ThrowException(ERR_LOAD_SQLITE_EXTENSION(isolate, errmsg));
  }
}

void Database::SetAuthorizer(const FunctionCallbackInfo<Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  if (args[0]->IsNull()) {
    // Clear the authorizer
    sqlite3_set_authorizer(db->connection_, nullptr, nullptr);
    db->object()->SetInternalField(kAuthorizerCallback, Null(isolate));
    return;
  }

  if (!args[0]->IsFunction()) {
    THROW_ERR_INVALID_ARG_TYPE(
        isolate, "The \"callback\" argument must be a function or null.");
    return;
  }

  Local<Function> fn = args[0].As<Function>();

  db->object()->SetInternalField(kAuthorizerCallback, fn);

  int r =
      sqlite3_set_authorizer(db->connection_, Database::AuthorizerCallback, db);

  if (r != SQLITE_OK) {
    CHECK_ERROR_OR_THROW(isolate, db, r, SQLITE_OK, void());
  }
}

int Database::AuthorizerCallback(void* user_data,
                                 int action_code,
                                 const char* param1,
                                 const char* param2,
                                 const char* param3,
                                 const char* param4) {
  Database* db = static_cast<Database*>(user_data);
  Environment* env = db->env();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();

  Local<Value> cb =
      db->object()->GetInternalField(kAuthorizerCallback).template As<Value>();

  CHECK(cb->IsFunction());

  Local<Function> callback = cb.As<Function>();
  LocalVector<Value> js_argv(isolate);

  // Convert SQLite authorizer parameters to JavaScript values
  js_argv.emplace_back(Integer::New(isolate, action_code));
  js_argv.emplace_back(
      NullableSQLiteStringToValue(isolate, param1).ToLocalChecked());
  js_argv.emplace_back(
      NullableSQLiteStringToValue(isolate, param2).ToLocalChecked());
  js_argv.emplace_back(
      NullableSQLiteStringToValue(isolate, param3).ToLocalChecked());
  js_argv.emplace_back(
      NullableSQLiteStringToValue(isolate, param4).ToLocalChecked());

  MaybeLocal<Value> retval = callback->Call(
      context, Undefined(isolate), js_argv.size(), js_argv.data());

  Local<Value> result;

  if (!retval.ToLocal(&result)) {
    db->SetIgnoreNextSQLiteError(true);
    return SQLITE_DENY;
  }

  Local<String> error_message;

  if (!result->IsInt32()) {
    if (!String::NewFromUtf8(
             isolate,
             "Authorizer callback must return an integer authorization code")
             .ToLocal(&error_message)) {
      return SQLITE_DENY;
    }

    Local<Value> err = Exception::TypeError(error_message);
    isolate->ThrowException(err);
    db->SetIgnoreNextSQLiteError(true);
    return SQLITE_DENY;
  }

  int32_t int_result = result.As<Int32>()->Value();
  if (int_result != SQLITE_OK && int_result != SQLITE_DENY &&
      int_result != SQLITE_IGNORE) {
    if (!String::NewFromUtf8(
             isolate,
             "Authorizer callback returned a invalid authorization code")
             .ToLocal(&error_message)) {
      return SQLITE_DENY;
    }

    Local<Value> err = Exception::RangeError(error_message);
    isolate->ThrowException(err);
    db->SetIgnoreNextSQLiteError(true);
    return SQLITE_DENY;
  }

  return int_result;
}

Statement::Statement(Environment* env,
                     Local<Object> object,
                     BaseObjectPtr<Database> db,
                     sqlite3_stmt* stmt)
    : BaseObject(env, object), db_(std::move(db)) {
  MakeWeak();
  statement_ = stmt;
  use_big_ints_ = db_->use_big_ints();
  return_arrays_ = db_->return_arrays();
  allow_bare_named_params_ = db_->allow_bare_named_params();
  allow_unknown_named_params_ = db_->allow_unknown_named_params();

  bare_named_params_ = std::nullopt;
}

Statement::~Statement() {
  if (!IsFinalized()) {
    db_->UntrackStatement(this);
    Finalize();
  }
}

void Statement::Finalize() {
  sqlite3_finalize(statement_);
  statement_ = nullptr;
}

inline bool Statement::IsFinalized() {
  return statement_ == nullptr;
}

bool Statement::BindParams(const FunctionCallbackInfo<Value>& args) {
  int r = sqlite3_clear_bindings(statement_);
  CHECK_ERROR_OR_THROW(env()->isolate(), db_.get(), r, SQLITE_OK, false);

  int anon_idx = 1;
  int anon_start = 0;

  if (args[0]->IsObject() && !args[0]->IsArrayBufferView()) {
    Local<Object> obj = args[0].As<Object>();
    Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
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
          if (allow_unknown_named_params_) {
            continue;
          } else {
            THROW_ERR_INVALID_STATE(
                env(), "Unknown named parameter '%s'", utf8_key);
            return false;
          }
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
    while (1) {
      const char* param = sqlite3_bind_parameter_name(statement_, anon_idx);
      if (param == nullptr || param[0] == '?') break;
      anon_idx++;
    }

    if (!BindValue(args[i], anon_idx)) {
      return false;
    }

    anon_idx++;
  }

  return true;
}

bool Statement::BindValue(const Local<Value>& value, const int index) {
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
  } else if (value->IsArrayBufferView()) {
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

  CHECK_ERROR_OR_THROW(env()->isolate(), db_.get(), r, SQLITE_OK, false);
  return true;
}

MaybeLocal<Value> Statement::ColumnToValue(const int column) {
  return StatementExecutionHelper::ColumnToValue(
      env(), statement_, column, use_big_ints_);
}

MaybeLocal<Name> Statement::ColumnNameToName(const int column) {
  const char* col_name = sqlite3_column_name(statement_, column);
  if (col_name == nullptr) {
    THROW_ERR_INVALID_STATE(env(), "Cannot get name of column %d", column);
    return MaybeLocal<Name>();
  }

  return String::NewFromUtf8(env()->isolate(), col_name).As<Name>();
}

MaybeLocal<Value> StatementExecutionHelper::ColumnToValue(Environment* env,
                                                          sqlite3_stmt* stmt,
                                                          const int column,
                                                          bool use_big_ints) {
  Isolate* isolate = env->isolate();
  MaybeLocal<Value> js_val = MaybeLocal<Value>();
  SQLITE_VALUE_TO_JS(column, isolate, use_big_ints, js_val, stmt, column);
  return js_val;
}

MaybeLocal<Name> StatementExecutionHelper::ColumnNameToName(Environment* env,
                                                            sqlite3_stmt* stmt,
                                                            const int column) {
  const char* col_name = sqlite3_column_name(stmt, column);
  if (col_name == nullptr) {
    THROW_ERR_INVALID_STATE(env, "Cannot get name of column %d", column);
    return MaybeLocal<Name>();
  }

  return String::NewFromUtf8(env->isolate(), col_name).As<Name>();
}

void Statement::MemoryInfo(MemoryTracker* tracker) const {}

Maybe<void> ExtractRowValues(Environment* env,
                             sqlite3_stmt* stmt,
                             int num_cols,
                             bool use_big_ints,
                             LocalVector<Value>* row_values) {
  row_values->clear();
  row_values->reserve(num_cols);
  for (int i = 0; i < num_cols; ++i) {
    Local<Value> val;
    if (!StatementExecutionHelper::ColumnToValue(env, stmt, i, use_big_ints)
             .ToLocal(&val)) {
      return Nothing<void>();
    }
    row_values->emplace_back(val);
  }
  return JustVoid();
}

MaybeLocal<Value> StatementExecutionHelper::All(Environment* env,
                                                Database* db,
                                                sqlite3_stmt* stmt,
                                                bool return_arrays,
                                                bool use_big_ints) {
  Isolate* isolate = env->isolate();
  EscapableHandleScope scope(isolate);
  int r;
  int num_cols = sqlite3_column_count(stmt);
  LocalVector<Value> rows(isolate);
  LocalVector<Value> row_values(isolate);
  LocalVector<Name> row_keys(isolate);

  while ((r = sqlite3_step(stmt)) == SQLITE_ROW) {
    if (ExtractRowValues(env, stmt, num_cols, use_big_ints, &row_values)
            .IsNothing()) {
      return MaybeLocal<Value>();
    }

    if (return_arrays) {
      Local<Array> row_array =
          Array::New(isolate, row_values.data(), row_values.size());
      rows.emplace_back(row_array);
    } else {
      if (row_keys.size() == 0) {
        row_keys.reserve(num_cols);
        for (int i = 0; i < num_cols; ++i) {
          Local<Name> key;
          if (!ColumnNameToName(env, stmt, i).ToLocal(&key)) {
            return MaybeLocal<Value>();
          }
          row_keys.emplace_back(key);
        }
      }
      DCHECK_EQ(row_keys.size(), row_values.size());
      Local<Object> row_obj = Object::New(
          isolate, Null(isolate), row_keys.data(), row_values.data(), num_cols);
      rows.emplace_back(row_obj);
    }
  }

  CHECK_ERROR_OR_THROW(isolate, db, r, SQLITE_DONE, MaybeLocal<Value>());
  return scope.Escape(Array::New(isolate, rows.data(), rows.size()));
}

int StatementRun(sqlite3_stmt* stmt) {
  sqlite3_step(stmt);
  return sqlite3_reset(stmt);
}

MaybeLocal<Object> StatementSQLiteToJSConverter::ConvertStatementGet(
    Environment* env,
    sqlite3_stmt* stmt,
    int num_cols,
    bool use_big_ints,
    bool return_arrays) {
  Isolate* isolate = env->isolate();
  LocalVector<Value> row_values(isolate);
  if (ExtractRowValues(env, stmt, num_cols, use_big_ints, &row_values)
          .IsNothing()) {
    return MaybeLocal<Object>();
  }

  if (return_arrays) {
    return Array::New(isolate, row_values.data(), row_values.size());
  } else {
    LocalVector<Name> keys(isolate);
    keys.reserve(num_cols);
    for (int i = 0; i < num_cols; ++i) {
      Local<Name> key;
      if (!StatementExecutionHelper::ColumnNameToName(env, stmt, i)
               .ToLocal(&key)) {
        return MaybeLocal<Object>();
      }

      keys.emplace_back(key);
    }

    DCHECK_EQ(keys.size(), row_values.size());
    return Object::New(
        isolate, Null(isolate), keys.data(), row_values.data(), num_cols);
  }
}

MaybeLocal<Object> StatementSQLiteToJSConverter::ConvertStatementRun(
    Environment* env,
    bool use_big_ints,
    sqlite3_int64 changes,
    sqlite3_int64 last_insert_rowid) {
  Local<Object> result = Object::New(env->isolate());
  Local<Value> last_insert_rowid_val;
  Local<Value> changes_val;

  if (use_big_ints) {
    last_insert_rowid_val = BigInt::New(env->isolate(), last_insert_rowid);
    changes_val = BigInt::New(env->isolate(), changes);
  } else {
    last_insert_rowid_val =
        Number::New(env->isolate(), static_cast<double>(last_insert_rowid));
    changes_val = Number::New(env->isolate(), static_cast<double>(changes));
  }

  if (result
          ->Set(env->context(),
                env->last_insert_rowid_string(),
                last_insert_rowid_val)
          .IsNothing() ||
      result->Set(env->context(), env->changes_string(), changes_val)
          .IsNothing()) {
    return MaybeLocal<Object>();
  }

  return result;
}

MaybeLocal<Promise::Resolver> StatementAsyncExecutionHelper::All(
    Environment* env, Statement* stmt) {
  Isolate* isolate = env->isolate();
  Database* db = stmt->db_.get();
  auto task = [stmt]() -> std::vector<Row> {
    int num_cols = sqlite3_column_count(stmt->statement_);
    std::vector<Row> rows;

    auto dup_value = [&](int col) {
      return sqlite3_value_dup(sqlite3_column_value(stmt->statement_, col));
    };

    int r = 0;
    while ((r = sqlite3_step(stmt->statement_)) == SQLITE_ROW) {
      if (stmt->return_arrays_) {
        std::vector<sqlite3_value*> array_values;
        array_values.reserve(num_cols);
        for (int i = 0; i < num_cols; ++i) {
          array_values.emplace_back(dup_value(i));
        }

        rows.emplace_back(std::move(array_values));

      } else {
        RowObject object_values;
        object_values.reserve(num_cols);
        for (int i = 0; i < num_cols; ++i) {
          const char* col_name = sqlite3_column_name(stmt->statement_, i);
          object_values.emplace_back(std::string(col_name), dup_value(i));
        }

        rows.emplace_back(std::move(object_values));
      }
    }

    return rows;
  };

  auto after = [env, isolate, stmt](std::vector<Row> rows,
                                    Local<Promise::Resolver> resolver) {
    LocalVector<Value> js_rows(isolate);
    int i = 0;

    for (auto& row : rows) {
      if (std::holds_alternative<RowArray>(row)) {
        auto& arr = std::get<RowArray>(row);
        int num_cols = arr.size();
        LocalVector<Value> array_values(isolate);
        array_values.reserve(num_cols);
        for (sqlite3_value* sqlite_val : arr) {
          MaybeLocal<Value> js_val;
          SQLITE_VALUE_TO_JS(
              value, isolate, stmt->use_big_ints_, js_val, sqlite_val);
          if (js_val.IsEmpty()) {
            return;
          }

          Local<Value> v8Value;
          if (!js_val.ToLocal(&v8Value)) {
            return;
          }

          array_values.emplace_back(v8Value);
        }

        Local<Array> row_array =
            Array::New(isolate, array_values.data(), array_values.size());
        js_rows.emplace_back(row_array);
      } else {
        auto& object = std::get<RowObject>(row);
        int num_cols = object.size();
        LocalVector<Name> row_keys(isolate);
        row_keys.reserve(num_cols);
        LocalVector<Value> row_values(isolate);
        row_values.reserve(num_cols);
        for (auto& [key, sqlite_val] : object) {
          Local<Name> key_name;
          if (!String::NewFromUtf8(isolate, key.c_str()).ToLocal(&key_name)) {
            return;
          }

          row_keys.emplace_back(key_name);

          MaybeLocal<Value> js_val;
          SQLITE_VALUE_TO_JS(
              value, isolate, stmt->use_big_ints_, js_val, sqlite_val);
          if (js_val.IsEmpty()) {
            return;
          }

          Local<Value> v8Value;
          if (!js_val.ToLocal(&v8Value)) {
            return;
          }

          row_values.emplace_back(v8Value);
        }

        DCHECK_EQ(row_keys.size(), row_values.size());
        Local<Object> row_obj = Object::New(isolate,
                                            Null(isolate),
                                            row_keys.data(),
                                            row_values.data(),
                                            num_cols);
        js_rows.emplace_back(row_obj);
      }
    }

    resolver
        ->Resolve(env->context(),
                  Array::New(isolate, js_rows.data(), js_rows.size()))
        .FromJust();
  };

  Local<Promise::Resolver> resolver =
      MakeSQLiteAsyncWork<std::vector<Row>>(env, db, task, after);
  if (resolver.IsEmpty()) {
    return MaybeLocal<Promise::Resolver>();
  }

  return resolver;
}

MaybeLocal<Promise::Resolver> StatementAsyncExecutionHelper::Get(
    Environment* env, Statement* stmt) {
  Database* db = stmt->db_.get();
  auto task = [stmt]() -> std::tuple<int, int> {
    int r = sqlite3_step(stmt->statement_);
    if (r != SQLITE_ROW && r != SQLITE_DONE) {
      return std::make_tuple(r, 0);
    }
    return std::make_tuple(r, sqlite3_column_count(stmt->statement_));
  };

  auto after = [db, env, stmt](std::tuple<int, int> task_result,
                               Local<Promise::Resolver> resolver) {
    Isolate* isolate = env->isolate();
    auto [r, num_cols] = task_result;
    if (r == SQLITE_DONE) {
      resolver->Resolve(env->context(), Undefined(isolate)).FromJust();
      return;
    }

    if (r != SQLITE_ROW) {
      Local<Object> e;
      if (!CreateSQLiteError(isolate, db->Connection()).ToLocal(&e)) {
        return;
      }
      resolver->Reject(env->context(), e).FromJust();
      return;
    }

    if (num_cols == 0) {
      resolver->Resolve(env->context(), Undefined(isolate)).FromJust();
      return;
    }

    TryCatch try_catch(isolate);
    Local<Value> result;
    if (StatementSQLiteToJSConverter::ConvertStatementGet(env,
                                                          stmt->statement_,
                                                          num_cols,
                                                          stmt->use_big_ints_,
                                                          stmt->return_arrays_)
            .ToLocal(&result)) {
      resolver->Resolve(env->context(), result).FromJust();
      return;
    }

    if (try_catch.HasCaught()) {
      resolver->Reject(env->context(), try_catch.Exception()).FromJust();
    }
  };

  Local<Promise::Resolver> resolver =
      MakeSQLiteAsyncWork<std::tuple<int, int>>(env, db, task, after);
  if (resolver.IsEmpty()) {
    return MaybeLocal<Promise::Resolver>();
  }

  return resolver;
}

MaybeLocal<Promise::Resolver> StatementAsyncExecutionHelper::Run(
    Environment* env, Statement* stmt) {
  Database* db = stmt->db_.get();
  sqlite3* conn = db->Connection();
  auto task =
      [stmt,
       conn]() -> std::variant<int, std::tuple<sqlite3_int64, sqlite3_int64>> {
    sqlite3_step(stmt->statement_);
    int r = sqlite3_reset(stmt->statement_);
    if (r != SQLITE_OK) {
      return r;
    }

    sqlite3_int64 last_insert_rowid = sqlite3_last_insert_rowid(conn);
    sqlite3_int64 changes = sqlite3_changes64(conn);

    return std::make_tuple(last_insert_rowid, changes);
  };

  auto after =
      [env, stmt, conn](
          std::variant<int, std::tuple<sqlite3_int64, sqlite3_int64>> result,
          Local<Promise::Resolver> resolver) {
        if (std::holds_alternative<int>(result)) {
          Local<Object> e;
          if (!CreateSQLiteError(env->isolate(), conn).ToLocal(&e)) {
            return;
          }
          resolver->Reject(env->context(), e).FromJust();
          return;
        }

        auto [last_insert_rowid, changes] =
            std::get<std::tuple<sqlite3_int64, sqlite3_int64>>(result);

        Local<Object> promise_result;
        if (!StatementSQLiteToJSConverter::ConvertStatementRun(
                 env, stmt->use_big_ints_, changes, last_insert_rowid)
                 .ToLocal(&promise_result)) {
          return;
        }

        resolver->Resolve(env->context(), promise_result).FromJust();
      };

  Local<Promise::Resolver> resolver = MakeSQLiteAsyncWork<
      std::variant<int, std::tuple<sqlite3_int64, sqlite3_int64>>>(
      env, db, task, after);
  if (resolver.IsEmpty()) {
    return MaybeLocal<Promise::Resolver>();
  }

  return resolver;
}

MaybeLocal<Object> StatementExecutionHelper::Run(Environment* env,
                                                 Database* db,
                                                 sqlite3_stmt* stmt,
                                                 bool use_big_ints) {
  Isolate* isolate = env->isolate();
  sqlite3_step(stmt);
  int r = sqlite3_reset(stmt);
  CHECK_ERROR_OR_THROW(isolate, db, r, SQLITE_OK, Object::New(isolate));
  sqlite3_int64 last_insert_rowid = sqlite3_last_insert_rowid(db->Connection());
  sqlite3_int64 changes = sqlite3_changes64(db->Connection());
  Local<Object> result;
  if (!StatementSQLiteToJSConverter::ConvertStatementRun(
           env, use_big_ints, changes, last_insert_rowid)
           .ToLocal(&result)) {
    return Object::New(isolate);
  }

  return result;
}

BaseObjectPtr<StatementIterator> StatementExecutionHelper::Iterate(
    Environment* env, BaseObjectPtr<Statement> stmt) {
  Local<Context> context = env->context();
  Local<Object> global = context->Global();
  Local<Value> js_iterator;
  Local<Value> js_iterator_prototype;
  if (!global->Get(context, env->iterator_string()).ToLocal(&js_iterator)) {
    return BaseObjectPtr<StatementIterator>();
  }
  if (!js_iterator.As<Object>()
           ->Get(context, env->prototype_string())
           .ToLocal(&js_iterator_prototype)) {
    return BaseObjectPtr<StatementIterator>();
  }

  BaseObjectPtr<StatementIterator> iter = StatementIterator::Create(env, stmt);

  if (!iter) {
    // Error in iterator creation, likely already threw in Create
    return BaseObjectPtr<StatementIterator>();
  }

  if (iter->object()
          ->GetPrototypeV2()
          .As<Object>()
          ->SetPrototypeV2(context, js_iterator_prototype)
          .IsNothing()) {
    return BaseObjectPtr<StatementIterator>();
  }

  return iter;
}

MaybeLocal<Value> StatementExecutionHelper::Get(Environment* env,
                                                Database* db,
                                                sqlite3_stmt* stmt,
                                                bool return_arrays,
                                                bool use_big_ints) {
  Isolate* isolate = env->isolate();
  EscapableHandleScope scope(isolate);
  auto reset = OnScopeLeave([&]() { sqlite3_reset(stmt); });

  int r = sqlite3_step(stmt);
  if (r == SQLITE_DONE) return scope.Escape(Undefined(isolate));
  if (r != SQLITE_ROW) {
    THROW_ERR_SQLITE_ERROR(isolate, db);
    return MaybeLocal<Value>();
  }

  int num_cols = sqlite3_column_count(stmt);
  if (num_cols == 0) {
    return Undefined(isolate);
  }

  Local<Value> result;
  if (StatementSQLiteToJSConverter::ConvertStatementGet(
          env, stmt, num_cols, use_big_ints, return_arrays)
          .ToLocal(&result)) {
    return result;
  }

  return Undefined(isolate);
}

void Statement::All(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  Isolate* isolate = env->isolate();
  Database* db = stmt->db_.get();
  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(isolate, db, r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
  }

  if (!db->is_async()) {
    auto reset = OnScopeLeave([&]() { sqlite3_reset(stmt->statement_); });
    Local<Value> result;
    if (StatementExecutionHelper::All(env,
                                      stmt->db_.get(),
                                      stmt->statement_,
                                      stmt->return_arrays_,
                                      stmt->use_big_ints_)
            .ToLocal(&result)) {
      args.GetReturnValue().Set(result);
    }
    return;
  }

  Local<Promise::Resolver> resolver;
  if (StatementAsyncExecutionHelper::All(env, stmt).ToLocal(&resolver)) {
    args.GetReturnValue().Set(resolver->GetPromise());
  }
}

void Statement::Iterate(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(env->isolate(), stmt->db_.get(), r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
  }

  BaseObjectPtr<StatementIterator> iter =
      StatementExecutionHelper::Iterate(env, BaseObjectPtr<Statement>(stmt));

  if (!iter) {
    return;
  }

  args.GetReturnValue().Set(iter->object());
}

void Statement::Get(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  Database* db = stmt->db_.get();
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
  }

  if (!db->is_async()) {
    Local<Value> result;
    if (StatementExecutionHelper::Get(env,
                                      stmt->db_.get(),
                                      stmt->statement_,
                                      stmt->return_arrays_,
                                      stmt->use_big_ints_)
            .ToLocal(&result)) {
      args.GetReturnValue().Set(result);
    }

    return;
  }

  Local<Promise::Resolver> resolver;
  if (StatementAsyncExecutionHelper::Get(env, stmt).ToLocal(&resolver)) {
    args.GetReturnValue().Set(resolver->GetPromise());
  }
}

void Statement::Run(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  Database* db = stmt->db_.get();
  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
  }

  if (!db->is_async()) {
    Local<Object> result;
    if (StatementExecutionHelper::Run(
            env, stmt->db_.get(), stmt->statement_, stmt->use_big_ints_)
            .ToLocal(&result)) {
      args.GetReturnValue().Set(result);
    }

    return;
  }

  Local<Promise::Resolver> resolver;
  if (StatementAsyncExecutionHelper::Run(env, stmt).ToLocal(&resolver)) {
    args.GetReturnValue().Set(resolver->GetPromise());
  }
}

void Statement::Columns(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  int num_cols = sqlite3_column_count(stmt->statement_);
  Isolate* isolate = env->isolate();
  LocalVector<Value> cols(isolate);
  auto sqlite_column_template = env->sqlite_column_template();
  if (sqlite_column_template.IsEmpty()) {
    static constexpr std::string_view col_keys[] = {
        "column", "database", "name", "table", "type"};
    sqlite_column_template = DictionaryTemplate::New(isolate, col_keys);
    env->set_sqlite_column_template(sqlite_column_template);
  }

  cols.reserve(num_cols);
  for (int i = 0; i < num_cols; ++i) {
    MaybeLocal<Value> values[] = {
        NullableSQLiteStringToValue(
            isolate, sqlite3_column_origin_name(stmt->statement_, i)),
        NullableSQLiteStringToValue(
            isolate, sqlite3_column_database_name(stmt->statement_, i)),
        stmt->ColumnNameToName(i),
        NullableSQLiteStringToValue(
            isolate, sqlite3_column_table_name(stmt->statement_, i)),
        NullableSQLiteStringToValue(
            isolate, sqlite3_column_decltype(stmt->statement_, i)),
    };

    Local<Object> col;
    if (!NewDictionaryInstanceNullProto(
             env->context(), sqlite_column_template, values)
             .ToLocal(&col)) {
      return;
    }
    cols.emplace_back(col);
  }

  args.GetReturnValue().Set(Array::New(isolate, cols.data(), cols.size()));
}

void Statement::SourceSQLGetter(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
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

void Statement::ExpandedSQLGetter(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
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

void Statement::SetAllowBareNamedParameters(
    const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
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

void Statement::SetAllowUnknownNamedParameters(
    const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");

  if (!args[0]->IsBoolean()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"enabled\" argument must be a boolean.");
    return;
  }

  stmt->allow_unknown_named_params_ = args[0]->IsTrue();
}

void Statement::SetReadBigInts(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
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

void Statement::SetReturnArrays(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");

  if (!args[0]->IsBoolean()) {
    THROW_ERR_INVALID_ARG_TYPE(
        env->isolate(), "The \"returnArrays\" argument must be a boolean.");
    return;
  }

  stmt->return_arrays_ = args[0]->IsTrue();
}

void IllegalConstructor(const FunctionCallbackInfo<Value>& args) {
  THROW_ERR_ILLEGAL_CONSTRUCTOR(Environment::GetCurrent(args));
}

SQLTagStore::SQLTagStore(Environment* env,
                         Local<Object> object,
                         BaseObjectWeakPtr<Database> database,
                         int capacity)
    : BaseObject(env, object),
      database_(std::move(database)),
      sql_tags_(capacity),
      capacity_(capacity) {
  MakeWeak();
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

SQLTagStore::~SQLTagStore() {}

Local<FunctionTemplate> SQLTagStore::GetConstructorTemplate(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> tmpl =
      NewFunctionTemplate(isolate, IllegalConstructor);
  tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "SQLTagStore"));
  tmpl->InstanceTemplate()->SetInternalFieldCount(
      SQLTagStore::kInternalFieldCount);
  SetProtoMethod(isolate, tmpl, "get", Get);
  SetProtoMethod(isolate, tmpl, "all", All);
  SetProtoMethod(isolate, tmpl, "iterate", Iterate);
  SetProtoMethod(isolate, tmpl, "run", Run);
  SetProtoMethod(isolate, tmpl, "clear", Clear);
  SetProtoMethod(isolate, tmpl, "size", Size);
  SetSideEffectFreeGetter(
      isolate, tmpl, FIXED_ONE_BYTE_STRING(isolate, "capacity"), Capacity);
  SetSideEffectFreeGetter(
      isolate, tmpl, FIXED_ONE_BYTE_STRING(isolate, "db"), DatabaseGetter);
  return tmpl;
}

BaseObjectPtr<SQLTagStore> SQLTagStore::Create(
    Environment* env, BaseObjectWeakPtr<Database> database, int capacity) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }
  return MakeBaseObject<SQLTagStore>(env, obj, std::move(database), capacity);
}

void SQLTagStore::DatabaseGetter(const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* store;
  ASSIGN_OR_RETURN_UNWRAP(&store, args.This());
  args.GetReturnValue().Set(store->database_->object());
}

void SQLTagStore::Run(const FunctionCallbackInfo<Value>& info) {
  SQLTagStore* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, info.This());
  Environment* env = Environment::GetCurrent(info);

  THROW_AND_RETURN_ON_BAD_STATE(
      env, !session->database_->IsOpen(), "database is not open");

  BaseObjectPtr<Statement> stmt = PrepareStatement(info);

  if (!stmt) {
    return;
  }

  uint32_t n_params = info.Length() - 1;
  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(env->isolate(), stmt->db_.get(), r, SQLITE_OK, void());
  int param_count = sqlite3_bind_parameter_count(stmt->statement_);
  for (int i = 0; i < static_cast<int>(n_params) && i < param_count; ++i) {
    Local<Value> value = info[i + 1];
    if (!stmt->BindValue(value, i + 1)) {
      return;
    }
  }

  Local<Object> result;
  if (StatementExecutionHelper::Run(
          env, stmt->db_.get(), stmt->statement_, stmt->use_big_ints_)
          .ToLocal(&result)) {
    info.GetReturnValue().Set(result);
  }
}

void SQLTagStore::Iterate(const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_ON_BAD_STATE(
      env, !session->database_->IsOpen(), "database is not open");

  BaseObjectPtr<Statement> stmt = PrepareStatement(args);

  if (!stmt) {
    return;
  }

  uint32_t n_params = args.Length() - 1;
  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(env->isolate(), stmt->db_.get(), r, SQLITE_OK, void());
  int param_count = sqlite3_bind_parameter_count(stmt->statement_);
  for (int i = 0; i < static_cast<int>(n_params) && i < param_count; ++i) {
    Local<Value> value = args[i + 1];
    if (!stmt->BindValue(value, i + 1)) {
      return;
    }
  }

  BaseObjectPtr<StatementIterator> iter =
      StatementExecutionHelper::Iterate(env, BaseObjectPtr<Statement>(stmt));

  if (!iter) {
    return;
  }

  args.GetReturnValue().Set(iter->object());
}

void SQLTagStore::Get(const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_ON_BAD_STATE(
      env, !session->database_->IsOpen(), "database is not open");

  BaseObjectPtr<Statement> stmt = PrepareStatement(args);

  if (!stmt) {
    return;
  }

  uint32_t n_params = args.Length() - 1;
  Isolate* isolate = env->isolate();

  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(isolate, stmt->db_.get(), r, SQLITE_OK, void());

  int param_count = sqlite3_bind_parameter_count(stmt->statement_);
  for (int i = 0; i < static_cast<int>(n_params) && i < param_count; ++i) {
    Local<Value> value = args[i + 1];
    if (!stmt->BindValue(value, i + 1)) {
      return;
    }
  }

  Local<Value> result;
  if (StatementExecutionHelper::Get(env,
                                    stmt->db_.get(),
                                    stmt->statement_,
                                    stmt->return_arrays_,
                                    stmt->use_big_ints_)
          .ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

void SQLTagStore::All(const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_ON_BAD_STATE(
      env, !session->database_->IsOpen(), "database is not open");

  BaseObjectPtr<Statement> stmt = PrepareStatement(args);

  if (!stmt) {
    return;
  }

  uint32_t n_params = args.Length() - 1;
  Isolate* isolate = env->isolate();

  int r = sqlite3_reset(stmt->statement_);
  CHECK_ERROR_OR_THROW(isolate, stmt->db_.get(), r, SQLITE_OK, void());

  int param_count = sqlite3_bind_parameter_count(stmt->statement_);
  for (int i = 0; i < static_cast<int>(n_params) && i < param_count; ++i) {
    Local<Value> value = args[i + 1];
    if (!stmt->BindValue(value, i + 1)) {
      return;
    }
  }

  auto reset = OnScopeLeave([&]() { sqlite3_reset(stmt->statement_); });
  Local<Value> result;
  if (StatementExecutionHelper::All(env,
                                    stmt->db_.get(),
                                    stmt->statement_,
                                    stmt->return_arrays_,
                                    stmt->use_big_ints_)
          .ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

void SQLTagStore::Size(const FunctionCallbackInfo<Value>& info) {
  SQLTagStore* store;
  ASSIGN_OR_RETURN_UNWRAP(&store, info.This());
  info.GetReturnValue().Set(
      Integer::New(info.GetIsolate(), store->sql_tags_.Size()));
}

void SQLTagStore::Capacity(const FunctionCallbackInfo<Value>& info) {
  SQLTagStore* store;
  ASSIGN_OR_RETURN_UNWRAP(&store, info.This());
  info.GetReturnValue().Set(
      Integer::New(info.GetIsolate(), store->sql_tags_.Capacity()));
}

void SQLTagStore::Clear(const FunctionCallbackInfo<Value>& info) {
  SQLTagStore* store;
  ASSIGN_OR_RETURN_UNWRAP(&store, info.This());
  store->sql_tags_.Clear();
}

BaseObjectPtr<Statement> SQLTagStore::PrepareStatement(
    const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* session = BaseObject::FromJSObject<SQLTagStore>(args.This());
  if (!session) {
    THROW_ERR_INVALID_ARG_TYPE(
        Environment::GetCurrent(args)->isolate(),
        "This method can only be called on SQLTagStore instances.");
    return BaseObjectPtr<Statement>();
  }
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = isolate->GetCurrentContext();

  if (args.Length() < 1 || !args[0]->IsArray()) {
    THROW_ERR_INVALID_ARG_TYPE(
        isolate,
        "First argument must be an array of strings (template literal).");
    return BaseObjectPtr<Statement>();
  }

  Local<Array> strings = args[0].As<Array>();
  uint32_t n_strings = strings->Length();
  uint32_t n_params = args.Length() - 1;

  std::string sql;
  for (uint32_t i = 0; i < n_strings; ++i) {
    Local<Value> str_val;
    if (!strings->Get(context, i).ToLocal(&str_val) || !str_val->IsString()) {
      THROW_ERR_INVALID_ARG_TYPE(isolate,
                                 "Template literal parts must be strings.");
      return BaseObjectPtr<Statement>();
    }
    Utf8Value part(isolate, str_val);
    sql += part.ToStringView();
    if (i < n_params) {
      sql += "?";
    }
  }

  BaseObjectPtr<Statement> stmt = nullptr;
  if (session->sql_tags_.Exists(sql)) {
    stmt = session->sql_tags_.Get(sql);
    if (stmt->IsFinalized()) {
      session->sql_tags_.Erase(sql);
      stmt = nullptr;
    }
  }

  if (stmt == nullptr) {
    sqlite3_stmt* s = nullptr;
    int r = sqlite3_prepare_v2(
        session->database_->connection_, sql.data(), sql.size(), &s, 0);

    if (r != SQLITE_OK) {
      THROW_ERR_SQLITE_ERROR(isolate, "Failed to prepare statement");
      sqlite3_finalize(s);
      return BaseObjectPtr<Statement>();
    }

    BaseObjectPtr<Statement> stmt_obj =
        Statement::Create(env, BaseObjectPtr<Database>(session->database_), s);

    if (!stmt_obj) {
      THROW_ERR_SQLITE_ERROR(isolate, "Failed to create Statement");
      sqlite3_finalize(s);
      return BaseObjectPtr<Statement>();
    }

    session->sql_tags_.Put(sql, stmt_obj);
    stmt = stmt_obj;
  }

  return stmt;
}

void SQLTagStore::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize(MemoryInfoName(), SelfSize());
  tracker->TrackField("database", database_);
  size_t cache_content_size = 0;
  for (const auto& pair : sql_tags_) {
    cache_content_size += pair.first.capacity();
    cache_content_size += sizeof(pair.second);
  }
  tracker->TrackFieldWithSize("sql_tags_cache", cache_content_size);
}

BaseObjectPtr<Statement> Statement::Create(Environment* env,
                                           BaseObjectPtr<Database> db,
                                           sqlite3_stmt* stmt) {
  Local<FunctionTemplate> tmpl =
      env->sqlite_statement_async_constructor_template();
  if (!db->is_async()) {
    tmpl = env->sqlite_statement_sync_constructor_template();
  }
  Local<Object> obj;
  if (!tmpl->InstanceTemplate()->NewInstance(env->context()).ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<Statement>(env, obj, std::move(db), stmt);
}

StatementIterator::StatementIterator(Environment* env,
                                     Local<Object> object,
                                     BaseObjectPtr<Statement> stmt)
    : BaseObject(env, object), stmt_(std::move(stmt)) {
  MakeWeak();
  done_ = false;
}

StatementIterator::~StatementIterator() {}
void StatementIterator::MemoryInfo(MemoryTracker* tracker) const {}

Local<FunctionTemplate> StatementIterator::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl =
      env->sqlite_statement_sync_iterator_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "StatementIterator"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Statement::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "next", StatementIterator::Next);
    SetProtoMethod(isolate, tmpl, "return", StatementIterator::Return);
    env->set_sqlite_statement_sync_iterator_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<StatementIterator> StatementIterator::Create(
    Environment* env, BaseObjectPtr<Statement> stmt) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return BaseObjectPtr<StatementIterator>();
  }

  return MakeBaseObject<StatementIterator>(env, obj, std::move(stmt));
}

void StatementIterator::Next(const FunctionCallbackInfo<Value>& args) {
  StatementIterator* iter;
  ASSIGN_OR_RETURN_UNWRAP(&iter, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, iter->stmt_->IsFinalized(), "statement has been finalized");
  Isolate* isolate = env->isolate();

  auto iter_template = getLazyIterTemplate(env);

  if (iter->done_) {
    MaybeLocal<Value> values[]{
        Boolean::New(isolate, true),
        Null(isolate),
    };
    Local<Object> result;
    if (NewDictionaryInstanceNullProto(env->context(), iter_template, values)
            .ToLocal(&result)) {
      args.GetReturnValue().Set(result);
    }
    return;
  }

  int r = sqlite3_step(iter->stmt_->statement_);
  if (r != SQLITE_ROW) {
    CHECK_ERROR_OR_THROW(
        env->isolate(), iter->stmt_->db_.get(), r, SQLITE_DONE, void());
    sqlite3_reset(iter->stmt_->statement_);
    MaybeLocal<Value> values[] = {Boolean::New(isolate, true), Null(isolate)};
    Local<Object> result;
    if (NewDictionaryInstanceNullProto(env->context(), iter_template, values)
            .ToLocal(&result)) {
      args.GetReturnValue().Set(result);
    }
    return;
  }

  int num_cols = sqlite3_column_count(iter->stmt_->statement_);
  Local<Value> row_value;
  LocalVector<Name> row_keys(isolate);
  LocalVector<Value> row_values(isolate);

  if (ExtractRowValues(env,
                       iter->stmt_->statement_,
                       num_cols,
                       iter->stmt_->use_big_ints_,
                       &row_values)
          .IsNothing()) {
    return;
  }

  if (iter->stmt_->return_arrays_) {
    row_value = Array::New(isolate, row_values.data(), row_values.size());
  } else {
    row_keys.reserve(num_cols);
    for (int i = 0; i < num_cols; ++i) {
      Local<Name> key;
      if (!iter->stmt_->ColumnNameToName(i).ToLocal(&key)) return;
      row_keys.emplace_back(key);
    }

    DCHECK_EQ(row_keys.size(), row_values.size());
    row_value = Object::New(
        isolate, Null(isolate), row_keys.data(), row_values.data(), num_cols);
  }

  MaybeLocal<Value> values[] = {Boolean::New(isolate, false), row_value};
  Local<Object> result;
  if (NewDictionaryInstanceNullProto(env->context(), iter_template, values)
          .ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

void StatementIterator::Return(const FunctionCallbackInfo<Value>& args) {
  StatementIterator* iter;
  ASSIGN_OR_RETURN_UNWRAP(&iter, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, iter->stmt_->IsFinalized(), "statement has been finalized");
  Isolate* isolate = env->isolate();

  sqlite3_reset(iter->stmt_->statement_);
  iter->done_ = true;

  auto iter_template = getLazyIterTemplate(env);
  MaybeLocal<Value> values[] = {Boolean::New(isolate, true), Null(isolate)};

  Local<Object> result;
  if (NewDictionaryInstanceNullProto(env->context(), iter_template, values)
          .ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

Session::Session(Environment* env,
                 Local<Object> object,
                 BaseObjectWeakPtr<Database> database,
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
                                       BaseObjectWeakPtr<Database> database,
                                       sqlite3_session* session) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
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
    SetProtoDispose(isolate, tmpl, Session::Dispose);
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
  THROW_AND_RETURN_ON_BAD_STATE(
      env, !session->database_->IsOpen(), "database is not open");
  THROW_AND_RETURN_ON_BAD_STATE(
      env, session->session_ == nullptr, "session is not open");

  int nChangeset;
  void* pChangeset;
  int r = sqliteChangesetFunc(session->session_, &nChangeset, &pChangeset);
  CHECK_ERROR_OR_THROW(
      env->isolate(), session->database_.get(), r, SQLITE_OK, void());

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

void Session::Dispose(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch(args.GetIsolate());
  Close(args);
  if (try_catch.HasCaught()) {
    CHECK(try_catch.CanContinue());
  }
}

void Session::Delete() {
  if (!database_ || !database_->connection_ || session_ == nullptr) return;
  sqlite3session_delete(session_);
  database_->sessions_.erase(session_);
  session_ = nullptr;
}

void DefineConstants(Local<Object> target) {
  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_OMIT);
  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_REPLACE);
  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_ABORT);

  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_DATA);
  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_NOTFOUND);
  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_CONFLICT);
  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_CONSTRAINT);
  NODE_DEFINE_CONSTANT(target, SQLITE_CHANGESET_FOREIGN_KEY);

  // Authorization result codes
  NODE_DEFINE_CONSTANT(target, SQLITE_OK);
  NODE_DEFINE_CONSTANT(target, SQLITE_DENY);
  NODE_DEFINE_CONSTANT(target, SQLITE_IGNORE);

  // Authorization action codes
  NODE_DEFINE_CONSTANT(target, SQLITE_CREATE_INDEX);
  NODE_DEFINE_CONSTANT(target, SQLITE_CREATE_TABLE);
  NODE_DEFINE_CONSTANT(target, SQLITE_CREATE_TEMP_INDEX);
  NODE_DEFINE_CONSTANT(target, SQLITE_CREATE_TEMP_TABLE);
  NODE_DEFINE_CONSTANT(target, SQLITE_CREATE_TEMP_TRIGGER);
  NODE_DEFINE_CONSTANT(target, SQLITE_CREATE_TEMP_VIEW);
  NODE_DEFINE_CONSTANT(target, SQLITE_CREATE_TRIGGER);
  NODE_DEFINE_CONSTANT(target, SQLITE_CREATE_VIEW);
  NODE_DEFINE_CONSTANT(target, SQLITE_DELETE);
  NODE_DEFINE_CONSTANT(target, SQLITE_DROP_INDEX);
  NODE_DEFINE_CONSTANT(target, SQLITE_DROP_TABLE);
  NODE_DEFINE_CONSTANT(target, SQLITE_DROP_TEMP_INDEX);
  NODE_DEFINE_CONSTANT(target, SQLITE_DROP_TEMP_TABLE);
  NODE_DEFINE_CONSTANT(target, SQLITE_DROP_TEMP_TRIGGER);
  NODE_DEFINE_CONSTANT(target, SQLITE_DROP_TEMP_VIEW);
  NODE_DEFINE_CONSTANT(target, SQLITE_DROP_TRIGGER);
  NODE_DEFINE_CONSTANT(target, SQLITE_DROP_VIEW);
  NODE_DEFINE_CONSTANT(target, SQLITE_INSERT);
  NODE_DEFINE_CONSTANT(target, SQLITE_PRAGMA);
  NODE_DEFINE_CONSTANT(target, SQLITE_READ);
  NODE_DEFINE_CONSTANT(target, SQLITE_SELECT);
  NODE_DEFINE_CONSTANT(target, SQLITE_TRANSACTION);
  NODE_DEFINE_CONSTANT(target, SQLITE_UPDATE);
  NODE_DEFINE_CONSTANT(target, SQLITE_ATTACH);
  NODE_DEFINE_CONSTANT(target, SQLITE_DETACH);
  NODE_DEFINE_CONSTANT(target, SQLITE_ALTER_TABLE);
  NODE_DEFINE_CONSTANT(target, SQLITE_REINDEX);
  NODE_DEFINE_CONSTANT(target, SQLITE_ANALYZE);
  NODE_DEFINE_CONSTANT(target, SQLITE_CREATE_VTABLE);
  NODE_DEFINE_CONSTANT(target, SQLITE_DROP_VTABLE);
  NODE_DEFINE_CONSTANT(target, SQLITE_FUNCTION);
  NODE_DEFINE_CONSTANT(target, SQLITE_SAVEPOINT);
  NODE_DEFINE_CONSTANT(target, SQLITE_COPY);
  NODE_DEFINE_CONSTANT(target, SQLITE_RECURSIVE);
}

void DefineStatementMethods(Isolate* isolate, Local<FunctionTemplate> tmpl) {
  SetProtoMethod(isolate, tmpl, "iterate", Statement::Iterate);
  SetProtoMethod(isolate, tmpl, "all", Statement::All);
  SetProtoMethod(isolate, tmpl, "get", Statement::Get);
  SetProtoMethod(isolate, tmpl, "run", Statement::Run);
  SetProtoMethodNoSideEffect(isolate, tmpl, "columns", Statement::Columns);
  SetSideEffectFreeGetter(isolate,
                          tmpl,
                          FIXED_ONE_BYTE_STRING(isolate, "sourceSQL"),
                          Statement::SourceSQLGetter);
  SetSideEffectFreeGetter(isolate,
                          tmpl,
                          FIXED_ONE_BYTE_STRING(isolate, "expandedSQL"),
                          Statement::ExpandedSQLGetter);
  SetProtoMethod(isolate,
                 tmpl,
                 "setAllowBareNamedParameters",
                 Statement::SetAllowBareNamedParameters);
  SetProtoMethod(isolate,
                 tmpl,
                 "setAllowUnknownNamedParameters",
                 Statement::SetAllowUnknownNamedParameters);
  SetProtoMethod(isolate, tmpl, "setReadBigInts", Statement::SetReadBigInts);
  SetProtoMethod(isolate, tmpl, "setReturnArrays", Statement::SetReturnArrays);
}

inline void DefineStatementFunctionTemplates(Environment* env) {
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> sync_tmpl =
      env->sqlite_statement_sync_constructor_template();
  if (sync_tmpl.IsEmpty()) {
    sync_tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    sync_tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "StatementSync"));
    sync_tmpl->InstanceTemplate()->SetInternalFieldCount(
        Statement::kInternalFieldCount);
    DefineStatementMethods(isolate, sync_tmpl);
    env->set_sqlite_statement_sync_constructor_template(sync_tmpl);
  }

  Local<FunctionTemplate> async_tmpl =
      env->sqlite_statement_async_constructor_template();
  if (async_tmpl.IsEmpty()) {
    async_tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    async_tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "Statement"));
    async_tmpl->InstanceTemplate()->SetInternalFieldCount(
        Statement::kInternalFieldCount);
    DefineStatementMethods(isolate, async_tmpl);
    env->set_sqlite_statement_async_constructor_template(async_tmpl);
  }
}

inline void DefineDatabaseMethods(Isolate* isolate,
                                  Local<FunctionTemplate> tmpl) {
  SetProtoMethod(isolate, tmpl, "open", Database::Open);
  SetProtoMethod(isolate, tmpl, "close", Database::Close);
  SetProtoDispose(isolate, tmpl, Database::Dispose);
  SetProtoMethod(isolate, tmpl, "prepare", Database::Prepare);
  SetProtoMethod(isolate, tmpl, "exec", Database::Exec);
  SetProtoMethod(isolate, tmpl, "function", Database::CustomFunction);
  SetProtoMethod(isolate, tmpl, "createTagStore", Database::CreateTagStore);
  SetProtoMethodNoSideEffect(isolate, tmpl, "location", Database::Location);
  SetProtoMethod(isolate, tmpl, "aggregate", Database::AggregateFunction);
  SetProtoMethod(isolate, tmpl, "createSession", Database::CreateSession);
  SetProtoMethod(isolate, tmpl, "applyChangeset", Database::ApplyChangeset);
  SetProtoMethod(
      isolate, tmpl, "enableLoadExtension", Database::EnableLoadExtension);
  SetProtoMethod(isolate, tmpl, "enableDefensive", Database::EnableDefensive);
  SetProtoMethod(isolate, tmpl, "loadExtension", Database::LoadExtension);
  SetProtoMethod(isolate, tmpl, "setAuthorizer", Database::SetAuthorizer);
  SetSideEffectFreeGetter(isolate,
                          tmpl,
                          FIXED_ONE_BYTE_STRING(isolate, "isOpen"),
                          Database::IsOpenGetter);
  SetSideEffectFreeGetter(isolate,
                          tmpl,
                          FIXED_ONE_BYTE_STRING(isolate, "isTransaction"),
                          Database::IsTransactionGetter);
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  Local<FunctionTemplate> db_tmpl = NewFunctionTemplate(isolate, Database::New);
  db_tmpl->InstanceTemplate()->SetInternalFieldCount(
      Database::kInternalFieldCount);
  Local<FunctionTemplate> db_async_tmpl =
      NewFunctionTemplate(isolate, Database::NewAsync);
  db_async_tmpl->InstanceTemplate()->SetInternalFieldCount(
      Database::kInternalFieldCount);
  Local<Object> constants = Object::New(isolate);

  DefineConstants(constants);
  DefineStatementFunctionTemplates(env);
  DefineDatabaseMethods(isolate, db_tmpl);
  DefineDatabaseMethods(isolate, db_async_tmpl);

  Local<String> sqlite_type_key = FIXED_ONE_BYTE_STRING(isolate, "sqlite-type");
  Local<v8::Symbol> sqlite_type_symbol =
      v8::Symbol::For(isolate, sqlite_type_key);
  Local<String> database_sync_string =
      FIXED_ONE_BYTE_STRING(isolate, "node:sqlite");
  db_tmpl->InstanceTemplate()->Set(sqlite_type_symbol, database_sync_string);

  SetConstructorFunction(context, target, "DatabaseSync", db_tmpl);
  SetConstructorFunction(context,
                         target,
                         "StatementSync",
                         env->sqlite_statement_sync_constructor_template());

  SetConstructorFunction(context, target, "Database", db_async_tmpl);
  SetConstructorFunction(context,
                         target,
                         "Statement",
                         env->sqlite_statement_async_constructor_template());
  SetConstructorFunction(
      context, target, "Session", Session::GetConstructorTemplate(env));

  target->Set(context, env->constants_string(), constants).Check();

  Local<Function> backup_function;

  if (!Function::New(context, Backup, Local<Value>(), 2)
           .ToLocal(&backup_function)) {
    return;
  }
  backup_function->SetName(env->backup_string());

  target->Set(context, env->backup_string(), backup_function).Check();
}

}  // namespace sqlite
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(sqlite, node::sqlite::Initialize)
