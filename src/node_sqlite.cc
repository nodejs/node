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
#include "uv.h"
#include "v8-isolate.h"
#include "v8-local-handle.h"
#include "v8-maybe.h"
#include "v8-primitive.h"
#include "v8-template.h"

#include <array>
#include <cinttypes>
#include <cmath>
#include <concepts>
#include <limits>
#include <variant>

namespace node {
namespace sqlite {

using std::in_place_type;
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
using v8::Intercepted;
using v8::Isolate;
using v8::JustVoid;
using v8::Local;
using v8::LocalVector;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::NewStringType;
using v8::Nothing;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::Promise;
using v8::PropertyCallbackInfo;
using v8::PropertyHandlerFlags;
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

#define REJECT_AND_RETURN_ON_INVALID_STATE(env, args, condition, msg)          \
  do {                                                                         \
    if ((condition)) {                                                         \
      RejectErrInvalidState((env), (args), (msg));                             \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define REJECT_AND_RETURN_ON_INVALID_ARG_TYPE(env, args, condition, msg)       \
  do {                                                                         \
    if ((condition)) {                                                         \
      RejectErrInvalidArgType((env), (args), (msg));                           \
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

inline void SetSideEffectFreeGetter(Isolate* isolate,
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

// Helper function to find limit info from JS property name
static constexpr const LimitInfo* GetLimitInfoFromName(std::string_view name) {
  for (const auto& info : kLimitMapping) {
    if (name == info.js_name) {
      return &info;
    }
  }
  return nullptr;
}

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

inline void THROW_ERR_SQLITE_ERROR(Isolate* isolate, sqlite3* db) {
  Local<Object> e;
  if (CreateSQLiteError(isolate, db).ToLocal(&e)) {
    isolate->ThrowException(e);
  }
}
inline void THROW_ERR_SQLITE_ERROR(Isolate* isolate, DatabaseSync* db) {
  if (db->ShouldIgnoreSQLiteError()) {
    db->SetIgnoreNextSQLiteError(false);
    return;
  }

  THROW_ERR_SQLITE_ERROR(isolate, db->Connection());
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

inline void RejectErr(Environment* env,
                      const FunctionCallbackInfo<Value>& args,
                      Local<Value> error) {
  Local<Context> context = env->context();
  auto resolver = Promise::Resolver::New(context).ToLocalChecked();
  resolver->Reject(context, error).ToChecked();
  args.GetReturnValue().Set(resolver->GetPromise());
}

template <typename ErrFactory>
  requires requires(ErrFactory err_factory, Isolate* isolate) {
             { err_factory(isolate) } -> std::convertible_to<Local<Value>>;
           }
inline void RejectErr(Environment* env,
                      const FunctionCallbackInfo<Value>& args,
                      ErrFactory&& err_factory) {
  Local<Value> error = err_factory(env->isolate());
  RejectErr(env, args, error);
}

inline void RejectErrInvalidState(Environment* env,
                                  const FunctionCallbackInfo<Value>& args,
                                  std::string_view message) {
  RejectErr(env, args, [&](Isolate* isolate) -> Local<Object> {
    return ERR_INVALID_STATE(isolate, message);
  });
}

inline void RejectErrInvalidArgType(Environment* env,
                                    const FunctionCallbackInfo<Value>& args,
                                    std::string_view message) {
  RejectErr(env, args, [&](Isolate* isolate) -> Local<Object> {
    return ERR_INVALID_ARG_TYPE(isolate, message);
  });
}

inline void RejectWithSQLiteError(Environment* env,
                                  Local<Promise::Resolver> resolver,
                                  sqlite3* db) {
  resolver
      ->Reject(env->context(),
               CreateSQLiteError(env->isolate(), db).ToLocalChecked())
      .Check();
}
inline void RejectWithSQLiteError(Environment* env,
                                  Local<Promise::Resolver> resolver,
                                  int errcode) {
  resolver
      ->Reject(env->context(),
               CreateSQLiteError(env->isolate(), errcode).ToLocalChecked())
      .Check();
}

inline MaybeLocal<Value> NullableSQLiteStringToValue(Isolate* isolate,
                                                     const char* str) {
  if (str == nullptr) {
    return Null(isolate);
  }

  return String::NewFromUtf8(isolate, str, NewStringType::kInternalized)
      .As<Value>();
}

DatabaseCommon::DatabaseCommon(Environment* env,
                               Local<Object> object,
                               DatabaseOpenConfiguration&& open_config,
                               bool allow_load_extension)
    : BaseObject(env, object),
      open_config_(std::move(open_config)),
      allow_load_extension_(allow_load_extension),
      enable_load_extension_(allow_load_extension) {
  MakeWeak();
}

namespace {
void AddDatabaseCommonMethodsToTemplate(Isolate* isolate,
                                        Local<FunctionTemplate> tmpl) {
  SetProtoMethod(isolate, tmpl, "open", DatabaseCommon::Open);
  SetSideEffectFreeGetter(isolate,
                          tmpl,
                          FIXED_ONE_BYTE_STRING(isolate, "isOpen"),
                          DatabaseCommon::IsOpenGetter);
}
}  // namespace

class CustomAggregate {
 public:
  explicit CustomAggregate(Environment* env,
                           DatabaseSync* db,
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
    js_argv.reserve(argc + 1);

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
  DatabaseSync* db_;
  bool use_bigint_args_;
  Global<Value> start_;
  Global<Function> step_fn_;
  Global<Function> inverse_fn_;
  Global<Function> result_fn_;
};

class BackupJob : public ThreadPoolWork {
 public:
  explicit BackupJob(Environment* env,
                     DatabaseSync* source,
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
  DatabaseSync* source_;
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
                                         DatabaseSync* db,
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
  js_argv.reserve(argc);

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

DatabaseSyncLimits::DatabaseSyncLimits(Environment* env,
                                       Local<Object> object,
                                       BaseObjectWeakPtr<DatabaseSync> database)
    : BaseObject(env, object), database_(std::move(database)) {
  MakeWeak();
}

DatabaseSyncLimits::~DatabaseSyncLimits() = default;

void DatabaseSyncLimits::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("database", database_);
}

Local<ObjectTemplate> DatabaseSyncLimits::GetTemplate(Environment* env) {
  Local<ObjectTemplate> tmpl = env->sqlite_limits_template();
  if (!tmpl.IsEmpty()) return tmpl;

  Isolate* isolate = env->isolate();
  tmpl = ObjectTemplate::New(isolate);
  tmpl->SetInternalFieldCount(DatabaseSyncLimits::kInternalFieldCount);
  tmpl->SetHandler(NamedPropertyHandlerConfiguration(
      LimitsGetter,
      LimitsSetter,
      LimitsQuery,
      nullptr,  // deleter - not allowed
      LimitsEnumerator,
      nullptr,  // definer
      nullptr,
      Local<Value>(),
      PropertyHandlerFlags::kHasNoSideEffect));

  env->set_sqlite_limits_template(tmpl);
  return tmpl;
}

Intercepted DatabaseSyncLimits::LimitsGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  // Skip symbols
  if (!property->IsString()) {
    return Intercepted::kNo;
  }

  DatabaseSyncLimits* limits;
  ASSIGN_OR_RETURN_UNWRAP(&limits, info.This(), Intercepted::kNo);

  Environment* env = limits->env();
  Isolate* isolate = env->isolate();

  Utf8Value prop_name(isolate, property);
  const LimitInfo* limit_info = GetLimitInfoFromName(prop_name.ToStringView());

  if (limit_info == nullptr) {
    return Intercepted::kNo;  // Unknown property, let default handling occur
  }

  if (!limits->database_ || !limits->database_->IsOpen()) {
    THROW_ERR_INVALID_STATE(env, "database is not open");
    return Intercepted::kYes;
  }

  int current_value = sqlite3_limit(
      limits->database_->Connection(), limit_info->sqlite_limit_id, -1);
  info.GetReturnValue().Set(Integer::New(isolate, current_value));
  return Intercepted::kYes;
}

Intercepted DatabaseSyncLimits::LimitsSetter(
    Local<Name> property,
    Local<Value> value,
    const PropertyCallbackInfo<void>& info) {
  if (!property->IsString()) {
    return Intercepted::kNo;
  }

  DatabaseSyncLimits* limits;
  ASSIGN_OR_RETURN_UNWRAP(&limits, info.This(), Intercepted::kNo);

  Environment* env = limits->env();
  Isolate* isolate = env->isolate();

  Utf8Value prop_name(isolate, property);
  const LimitInfo* limit_info = GetLimitInfoFromName(prop_name.ToStringView());

  if (limit_info == nullptr) {
    return Intercepted::kNo;
  }

  if (!limits->database_ || !limits->database_->IsOpen()) {
    THROW_ERR_INVALID_STATE(env, "database is not open");
    return Intercepted::kYes;
  }

  if (!value->IsNumber()) {
    THROW_ERR_INVALID_ARG_TYPE(
        isolate, "Limit value must be a non-negative integer or Infinity.");
    return Intercepted::kYes;
  }

  const double num_value = value.As<Number>()->Value();
  int new_value;

  if (std::isinf(num_value) && num_value > 0) {
    // Positive Infinity resets the limit to the compile-time maximum
    new_value = std::numeric_limits<int>::max();
  } else if (!value->IsInt32()) {
    THROW_ERR_INVALID_ARG_TYPE(
        isolate, "Limit value must be a non-negative integer or Infinity.");
    return Intercepted::kYes;
  } else {
    new_value = value.As<Int32>()->Value();
    if (new_value < 0) {
      THROW_ERR_OUT_OF_RANGE(isolate, "Limit value must be non-negative.");
      return Intercepted::kYes;
    }
  }

  sqlite3_limit(
      limits->database_->Connection(), limit_info->sqlite_limit_id, new_value);
  return Intercepted::kYes;
}

Intercepted DatabaseSyncLimits::LimitsQuery(
    Local<Name> property, const PropertyCallbackInfo<Integer>& info) {
  if (!property->IsString()) {
    return Intercepted::kNo;
  }

  Isolate* isolate = info.GetIsolate();
  Utf8Value prop_name(isolate, property);
  const LimitInfo* limit_info = GetLimitInfoFromName(prop_name.ToStringView());

  if (!limit_info) {
    return Intercepted::kNo;
  }

  // Property exists and is writable
  info.GetReturnValue().Set(
      Integer::New(isolate, v8::PropertyAttribute::DontDelete));
  return Intercepted::kYes;
}

void DatabaseSyncLimits::LimitsEnumerator(
    const PropertyCallbackInfo<Array>& info) {
  Isolate* isolate = info.GetIsolate();
  LocalVector<Value> names(isolate);

  for (const auto& [js_name, sqlite_limit_id] : kLimitMapping) {
    Local<String> name;
    if (!String::NewFromUtf8(
             isolate, js_name.data(), NewStringType::kNormal, js_name.size())
             .ToLocal(&name)) {
      return;
    }
    names.push_back(name);
  }

  info.GetReturnValue().Set(Array::New(isolate, names.data(), names.size()));
}

BaseObjectPtr<DatabaseSyncLimits> DatabaseSyncLimits::Create(
    Environment* env, BaseObjectWeakPtr<DatabaseSync> database) {
  Local<Object> obj;
  if (!GetTemplate(env)->NewInstance(env->context()).ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<DatabaseSyncLimits>(env, obj, std::move(database));
}

DatabaseSync::DatabaseSync(Environment* env,
                           Local<Object> object,
                           DatabaseOpenConfiguration&& open_config,
                           bool open,
                           bool allow_load_extension)
    : DatabaseCommon(
          env, object, std::move(open_config), allow_load_extension) {
  MakeWeak();
  ignore_next_sqlite_error_ = false;

  if (open) {
    Open();
  }
}

void DatabaseSync::AddBackup(BackupJob* job) {
  backups_.insert(job);
}

void DatabaseSync::RemoveBackup(BackupJob* job) {
  backups_.erase(job);
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
  FinalizeBackups();

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

namespace {
v8::Local<v8::FunctionTemplate> CreateDatabaseSyncConstructorTemplate(
    Environment* env) {
  Isolate* isolate = env->isolate();

  Local<FunctionTemplate> tmpl =
      NewFunctionTemplate(isolate, DatabaseSync::New);
  tmpl->InstanceTemplate()->SetInternalFieldCount(
      DatabaseSync::kInternalFieldCount);

  AddDatabaseCommonMethodsToTemplate(isolate, tmpl);

  SetProtoMethod(isolate, tmpl, "close", DatabaseSync::Close);
  SetProtoDispose(isolate, tmpl, DatabaseSync::Dispose);
  SetProtoMethod(isolate, tmpl, "prepare", DatabaseSync::Prepare);
  SetProtoMethod(isolate, tmpl, "exec", DatabaseSync::Exec);
  SetProtoMethod(isolate, tmpl, "function", DatabaseSync::CustomFunction);
  SetProtoMethod(isolate, tmpl, "createTagStore", DatabaseSync::CreateTagStore);
  SetProtoMethodNoSideEffect(isolate, tmpl, "location", DatabaseSync::Location);
  SetProtoMethod(isolate, tmpl, "aggregate", DatabaseSync::AggregateFunction);
  SetProtoMethod(isolate, tmpl, "createSession", DatabaseSync::CreateSession);
  SetProtoMethod(isolate, tmpl, "applyChangeset", DatabaseSync::ApplyChangeset);
  SetProtoMethod(
      isolate, tmpl, "enableLoadExtension", DatabaseSync::EnableLoadExtension);
  SetProtoMethod(
      isolate, tmpl, "enableDefensive", DatabaseSync::EnableDefensive);
  SetProtoMethod(isolate, tmpl, "loadExtension", DatabaseSync::LoadExtension);
  SetProtoMethod(isolate, tmpl, "setAuthorizer", DatabaseSync::SetAuthorizer);
  SetSideEffectFreeGetter(isolate,
                          tmpl,
                          FIXED_ONE_BYTE_STRING(isolate, "isTransaction"),
                          DatabaseSync::IsTransactionGetter);
  SetSideEffectFreeGetter(isolate,
                          tmpl,
                          FIXED_ONE_BYTE_STRING(isolate, "limits"),
                          DatabaseSync::LimitsGetter);
  Local<String> sqlite_type_key = FIXED_ONE_BYTE_STRING(isolate, "sqlite-type");
  Local<v8::Symbol> sqlite_type_symbol =
      v8::Symbol::For(isolate, sqlite_type_key);
  Local<String> database_sync_string =
      FIXED_ONE_BYTE_STRING(isolate, "node:sqlite");
  tmpl->InstanceTemplate()->Set(sqlite_type_symbol, database_sync_string);

  return tmpl;
}
}  // namespace

bool DatabaseCommon::Open() {
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

  int defensive_enabled;
  r = sqlite3_db_config(connection_,
                        SQLITE_DBCONFIG_DEFENSIVE,
                        static_cast<int>(open_config_.get_enable_defensive()),
                        &defensive_enabled);
  CHECK_ERROR_OR_THROW(env()->isolate(), connection_, r, SQLITE_OK, false);
  CHECK_EQ(defensive_enabled, open_config_.get_enable_defensive());

  sqlite3_busy_timeout(connection_, open_config_.get_timeout());

  // Apply initial limits
  for (const auto& [js_name, sqlite_limit_id] : kLimitMapping) {
    const auto& limit_value = open_config_.initial_limits()[sqlite_limit_id];
    if (limit_value.has_value()) {
      sqlite3_limit(connection_, sqlite_limit_id, *limit_value);
    }
  }

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
        env()->isolate(), connection_, load_extension_ret, SQLITE_OK, false);
  }

  return true;
}

void DatabaseSync::FinalizeBackups() {
  for (auto backup : backups_) {
    backup->Cleanup();
  }

  backups_.clear();
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

inline bool DatabaseCommon::IsOpen() {
  return connection_ != nullptr;
}

inline sqlite3* DatabaseCommon::Connection() {
  return connection_;
}

void DatabaseSync::SetIgnoreNextSQLiteError(bool ignore) {
  ignore_next_sqlite_error_ = ignore;
}

bool DatabaseSync::ShouldIgnoreSQLiteError() {
  return ignore_next_sqlite_error_;
}

void DatabaseSync::CreateTagStore(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db = BaseObject::Unwrap<DatabaseSync>(args.This());
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
      SQLTagStore::Create(env, BaseObjectWeakPtr<DatabaseSync>(db), capacity);
  if (!session) {
    // Handle error if creation failed
    THROW_ERR_SQLITE_ERROR(env->isolate(), "Failed to create SQLTagStore");
    return;
  }
  args.GetReturnValue().Set(session->object());
}

namespace {
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
                             field_name);

  return std::nullopt;
}

bool ParseCommonDatabaseOptions(Environment* env,
                                Local<Object> options,
                                DatabaseOpenConfiguration& open_config,
                                bool& open,
                                bool& allow_load_extension) {
  Local<String> open_string = FIXED_ONE_BYTE_STRING(env->isolate(), "open");
  Local<Value> open_v;
  if (!options->Get(env->context(), open_string).ToLocal(&open_v)) {
    return false;
  }
  if (!open_v->IsUndefined()) {
    if (!open_v->IsBoolean()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(), "The \"options.open\" argument must be a boolean.");
      return false;
    }
    open = open_v.As<Boolean>()->Value();
  }

  Local<String> read_only_string =
      FIXED_ONE_BYTE_STRING(env->isolate(), "readOnly");
  Local<Value> read_only_v;
  if (!options->Get(env->context(), read_only_string).ToLocal(&read_only_v)) {
    return false;
  }
  if (!read_only_v->IsUndefined()) {
    if (!read_only_v->IsBoolean()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(),
          "The \"options.readOnly\" argument must be a boolean.");
      return false;
    }
    open_config.set_read_only(read_only_v.As<Boolean>()->Value());
  }

  Local<String> enable_foreign_keys_string =
      FIXED_ONE_BYTE_STRING(env->isolate(), "enableForeignKeyConstraints");
  Local<Value> enable_foreign_keys_v;
  if (!options->Get(env->context(), enable_foreign_keys_string)
           .ToLocal(&enable_foreign_keys_v)) {
    return false;
  }
  if (!enable_foreign_keys_v->IsUndefined()) {
    if (!enable_foreign_keys_v->IsBoolean()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(),
          "The \"options.enableForeignKeyConstraints\" argument must be a "
          "boolean.");
      return false;
    }
    open_config.set_enable_foreign_keys(
        enable_foreign_keys_v.As<Boolean>()->Value());
  }

  Local<String> enable_dqs_string =
      FIXED_ONE_BYTE_STRING(env->isolate(), "enableDoubleQuotedStringLiterals");
  Local<Value> enable_dqs_v;
  if (!options->Get(env->context(), enable_dqs_string).ToLocal(&enable_dqs_v)) {
    return false;
  }
  if (!enable_dqs_v->IsUndefined()) {
    if (!enable_dqs_v->IsBoolean()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(),
          "The \"options.enableDoubleQuotedStringLiterals\" argument must be "
          "a boolean.");
      return false;
    }
    open_config.set_enable_dqs(enable_dqs_v.As<Boolean>()->Value());
  }

  Local<String> allow_extension_string =
      FIXED_ONE_BYTE_STRING(env->isolate(), "allowExtension");
  Local<Value> allow_extension_v;
  if (!options->Get(env->context(), allow_extension_string)
           .ToLocal(&allow_extension_v)) {
    return false;
  }

  if (!allow_extension_v->IsUndefined()) {
    if (!allow_extension_v->IsBoolean()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(),
          "The \"options.allowExtension\" argument must be a boolean.");
      return false;
    }
    allow_load_extension = allow_extension_v.As<Boolean>()->Value();
  }

  Local<Value> timeout_v;
  if (!options->Get(env->context(), env->timeout_string())
           .ToLocal(&timeout_v)) {
    return false;
  }

  if (!timeout_v->IsUndefined()) {
    if (!timeout_v->IsInt32()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(),
          "The \"options.timeout\" argument must be an integer.");
      return false;
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
        return false;
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
        return false;
      }
      open_config.set_return_arrays(return_arrays_v.As<Boolean>()->Value());
    }
  }

  Local<Value> allow_bare_named_params_v;
  if (options->Get(env->context(), env->allow_bare_named_params_string())
          .ToLocal(&allow_bare_named_params_v)) {
    if (!allow_bare_named_params_v->IsUndefined()) {
      if (!allow_bare_named_params_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                                   R"(The "options.allowBareNamedParameters" )"
                                   "argument must be a boolean.");
        return false;
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
        return false;
      }
      open_config.set_allow_unknown_named_params(
          allow_unknown_named_params_v.As<Boolean>()->Value());
    }
  }

  Local<Value> defensive_v;
  if (!options->Get(env->context(), env->defensive_string())
           .ToLocal(&defensive_v)) {
    return false;
  }
  if (!defensive_v->IsUndefined()) {
    if (!defensive_v->IsBoolean()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(),
          "The \"options.defensive\" argument must be a boolean.");
      return false;
    }
    open_config.set_enable_defensive(defensive_v.As<Boolean>()->Value());
  }

  // Parse limits option
  Local<Value> limits_v;
  if (!options->Get(env->context(), env->limits_string()).ToLocal(&limits_v)) {
    return false;
  }
  if (!limits_v->IsUndefined()) {
    if (!limits_v->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env->isolate(), "The \"options.limits\" argument must be an object.");
      return false;
    }

    Local<Object> limits_obj = limits_v.As<Object>();

    // Iterate through known limit names and extract values
    for (const auto& [js_name, sqlite_limit_id] : kLimitMapping) {
      Local<String> key;
      if (!String::NewFromUtf8(env->isolate(),
                               js_name.data(),
                               NewStringType::kNormal,
                               js_name.size())
               .ToLocal(&key)) {
        return false;
      }

      Local<Value> val;
      if (!limits_obj->Get(env->context(), key).ToLocal(&val)) {
        return false;
      }

      if (!val->IsUndefined()) {
        if (!val->IsInt32()) {
          std::string msg = "The \"options.limits." + std::string(js_name) +
                            "\" argument must be an integer.";
          THROW_ERR_INVALID_ARG_TYPE(env->isolate(), msg);
          return false;
        }

        int limit_val = val.As<Int32>()->Value();
        if (limit_val < 0) {
          std::string msg = "The \"options.limits." + std::string(js_name) +
                            "\" argument must be non-negative.";
          THROW_ERR_OUT_OF_RANGE(env->isolate(), msg);
          return false;
        }

        open_config.set_initial_limit(sqlite_limit_id, limit_val);
      }
    }
  }
  return true;
}
}  // namespace

void DatabaseSync::New(const FunctionCallbackInfo<Value>& args) {
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
    if (!ParseCommonDatabaseOptions(
            env, options, open_config, open, allow_load_extension)) {
      return;
    }
  }

  new DatabaseSync(
      env, args.This(), std::move(open_config), open, allow_load_extension);
}

void DatabaseCommon::Open(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  db->Open();
}

void DatabaseCommon::IsOpenGetter(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  args.GetReturnValue().Set(db->IsOpen());
}

void DatabaseSync::IsTransactionGetter(
    const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");
  args.GetReturnValue().Set(sqlite3_get_autocommit(db->connection_) == 0);
}

void DatabaseSync::LimitsGetter(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);

  Local<Value> limits_val =
      db->object()->GetInternalField(kLimitsObject).template As<Value>();

  if (limits_val->IsUndefined()) {
    BaseObjectPtr<DatabaseSyncLimits> limits =
        DatabaseSyncLimits::Create(env, BaseObjectWeakPtr<DatabaseSync>(db));
    if (limits) {
      db->object()->SetInternalField(kLimitsObject, limits->object());
      args.GetReturnValue().Set(limits->object());
    }
  } else {
    args.GetReturnValue().Set(limits_val);
  }
}

void DatabaseSync::Close(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");
  db->FinalizeStatements();
  db->DeleteSessions();
  int r = sqlite3_close_v2(db->connection_);
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());
  db->connection_ = nullptr;
}

void DatabaseSync::Dispose(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch(args.GetIsolate());
  Close(args);
  if (try_catch.HasCaught()) {
    CHECK(try_catch.CanContinue());
  }
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

  std::optional<bool> return_arrays;
  std::optional<bool> use_big_ints;
  std::optional<bool> allow_bare_named_params;
  std::optional<bool> allow_unknown_named_params;

  if (args.Length() > 1 && !args[1]->IsUndefined()) {
    if (!args[1]->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                                 "The \"options\" argument must be an object.");
      return;
    }
    Local<Object> options = args[1].As<Object>();

    Local<Value> return_arrays_v;
    if (!options
             ->Get(env->context(),
                   FIXED_ONE_BYTE_STRING(env->isolate(), "returnArrays"))
             .ToLocal(&return_arrays_v)) {
      return;
    }
    if (!return_arrays_v->IsUndefined()) {
      if (!return_arrays_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.returnArrays\" argument must be a boolean.");
        return;
      }
      return_arrays = return_arrays_v->IsTrue();
    }

    Local<Value> read_big_ints_v;
    if (!options
             ->Get(env->context(),
                   FIXED_ONE_BYTE_STRING(env->isolate(), "readBigInts"))
             .ToLocal(&read_big_ints_v)) {
      return;
    }
    if (!read_big_ints_v->IsUndefined()) {
      if (!read_big_ints_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.readBigInts\" argument must be a boolean.");
        return;
      }
      use_big_ints = read_big_ints_v->IsTrue();
    }

    Local<Value> allow_bare_named_params_v;
    if (!options
             ->Get(env->context(),
                   FIXED_ONE_BYTE_STRING(env->isolate(),
                                         "allowBareNamedParameters"))
             .ToLocal(&allow_bare_named_params_v)) {
      return;
    }
    if (!allow_bare_named_params_v->IsUndefined()) {
      if (!allow_bare_named_params_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.allowBareNamedParameters\" argument must be a "
            "boolean.");
        return;
      }
      allow_bare_named_params = allow_bare_named_params_v->IsTrue();
    }

    Local<Value> allow_unknown_named_params_v;
    if (!options
             ->Get(env->context(),
                   FIXED_ONE_BYTE_STRING(env->isolate(),
                                         "allowUnknownNamedParameters"))
             .ToLocal(&allow_unknown_named_params_v)) {
      return;
    }
    if (!allow_unknown_named_params_v->IsUndefined()) {
      if (!allow_unknown_named_params_v->IsBoolean()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env->isolate(),
            "The \"options.allowUnknownNamedParameters\" argument must be a "
            "boolean.");
        return;
      }
      allow_unknown_named_params = allow_unknown_named_params_v->IsTrue();
    }
  }

  Utf8Value sql(env->isolate(), args[0].As<String>());
  sqlite3_stmt* s = nullptr;
  int r = sqlite3_prepare_v2(db->connection_, *sql, -1, &s, nullptr);

  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());
  BaseObjectPtr<StatementSync> stmt =
      StatementSync::Create(env, BaseObjectPtr<DatabaseSync>(db), s);
  db->statements_.insert(stmt.get());

  if (return_arrays.has_value()) {
    stmt->return_arrays_ = return_arrays.value();
  }
  if (use_big_ints.has_value()) {
    stmt->use_big_ints_ = use_big_ints.value();
  }
  if (allow_bare_named_params.has_value()) {
    stmt->allow_bare_named_params_ = allow_bare_named_params.value();
  }
  if (allow_unknown_named_params.has_value()) {
    stmt->allow_unknown_named_params_ = allow_unknown_named_params.value();
  }

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
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());
}

void DatabaseSync::CustomFunction(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
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

void DatabaseSync::Location(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
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

void DatabaseSync::AggregateFunction(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
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

  DatabaseSync* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  THROW_AND_RETURN_ON_BAD_STATE(env, !db->IsOpen(), "database is not open");

  sqlite3_session* pSession;
  int r = sqlite3session_create(db->connection_, db_name.c_str(), &pSession);
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());
  db->sessions_.insert(pSession);

  r = sqlite3session_attach(pSession, table == "" ? nullptr : table.c_str());
  CHECK_ERROR_OR_THROW(env->isolate(), db, r, SQLITE_OK, void());

  BaseObjectPtr<Session> session =
      Session::Create(env, BaseObjectWeakPtr<DatabaseSync>(db), pSession);
  args.GetReturnValue().Set(session->object());
}

void Backup(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1 || !args[0]->IsObject()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"sourceDb\" argument must be an object.");
    return;
  }

  DatabaseSync* db;
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

void DatabaseSync::ApplyChangeset(const FunctionCallbackInfo<Value>& args) {
  ConflictCallbackContext context;

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

      context.filterCallback =
          [env, db, filterFunc](std::string_view item) -> bool {
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

void DatabaseSync::EnableLoadExtension(
    const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
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

void DatabaseSync::EnableDefensive(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
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

void DatabaseSync::LoadExtension(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
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

void DatabaseSync::SetAuthorizer(const FunctionCallbackInfo<Value>& args) {
  DatabaseSync* db;
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

  int r = sqlite3_set_authorizer(
      db->connection_, DatabaseSync::AuthorizerCallback, db);

  if (r != SQLITE_OK) {
    CHECK_ERROR_OR_THROW(isolate, db, r, SQLITE_OK, void());
  }
}

int DatabaseSync::AuthorizerCallback(void* user_data,
                                     int action_code,
                                     const char* param1,
                                     const char* param2,
                                     const char* param3,
                                     const char* param4) {
  DatabaseSync* db = static_cast<DatabaseSync*>(user_data);
  Environment* env = db->env();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();

  Local<Value> cb =
      db->object()->GetInternalField(kAuthorizerCallback).template As<Value>();

  CHECK(cb->IsFunction());

  Local<Function> callback = cb.As<Function>();

  LocalVector<Value> js_argv(
      isolate,
      {
          Integer::New(isolate, action_code),
          NullableSQLiteStringToValue(isolate, param1).ToLocalChecked(),
          NullableSQLiteStringToValue(isolate, param2).ToLocalChecked(),
          NullableSQLiteStringToValue(isolate, param3).ToLocalChecked(),
          NullableSQLiteStringToValue(isolate, param4).ToLocalChecked(),
      });

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

StatementSync::StatementSync(Environment* env,
                             Local<Object> object,
                             BaseObjectPtr<DatabaseSync> db,
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

inline int StatementSync::ResetStatement() {
  reset_generation_++;
  return sqlite3_reset(statement_);
}

bool StatementSync::BindParams(const FunctionCallbackInfo<Value>& args) {
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

bool StatementSync::BindValue(const Local<Value>& value, const int index) {
  // SQLite only supports a subset of JavaScript types. Some JS types such as
  // functions don't make sense to support. Other JS types such as booleans and
  // Dates could be supported by converting them to numbers. However, there
  // would not be a good way to read the values back from SQLite with the
  // original type.
  Isolate* isolate = env()->isolate();
  int r;
  if (value->IsNumber()) {
    const double val = value.As<Number>()->Value();
    r = sqlite3_bind_double(statement_, index, val);
  } else if (value->IsString()) {
    Utf8Value val(isolate, value.As<String>());
    if (val.IsAllocated()) {
      // Avoid an extra SQLite copy for large strings by transferring ownership
      // of the malloc()'d buffer to SQLite.
      char* data = *val;
      const sqlite3_uint64 length = static_cast<sqlite3_uint64>(val.length());
      val.Release();
      r = sqlite3_bind_text64(
          statement_, index, data, length, std::free, SQLITE_UTF8);
    } else {
      r = sqlite3_bind_text64(statement_,
                              index,
                              *val,
                              static_cast<sqlite3_uint64>(val.length()),
                              SQLITE_TRANSIENT,
                              SQLITE_UTF8);
    }
  } else if (value->IsNull()) {
    r = sqlite3_bind_null(statement_, index);
  } else if (value->IsArrayBufferView()) {
    ArrayBufferViewContents<uint8_t> buf(value);
    r = sqlite3_bind_blob64(statement_,
                            index,
                            buf.data(),
                            static_cast<sqlite3_uint64>(buf.length()),
                            SQLITE_TRANSIENT);
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
        isolate,
        "Provided value cannot be bound to SQLite parameter %d.",
        index);
    return false;
  }

  CHECK_ERROR_OR_THROW(isolate, db_.get(), r, SQLITE_OK, false);
  return true;
}

MaybeLocal<Value> StatementSync::ColumnToValue(const int column) {
  return StatementExecutionHelper::ColumnToValue(
      env(), statement_, column, use_big_ints_);
}

MaybeLocal<Name> StatementSync::ColumnNameToName(const int column) {
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

void StatementSync::MemoryInfo(MemoryTracker* tracker) const {}

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
                                                DatabaseSync* db,
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

namespace {
inline Local<DictionaryTemplate> GetRunResultTemplate(Environment* env,
                                                      Isolate* isolate) {
  auto run_result_template = env->sqlite_run_result_template();
  if (run_result_template.IsEmpty()) {
    static constexpr std::string_view run_result_keys[] = {"changes",
                                                           "lastInsertRowid"};
    run_result_template = DictionaryTemplate::New(isolate, run_result_keys);
    env->set_sqlite_run_result_template(run_result_template);
  }
  return run_result_template;
}

inline MaybeLocal<Object> CreateRunResultObject(Environment* env,
                                                sqlite3_int64 changes,
                                                sqlite3_int64 last_insert_rowid,
                                                bool use_big_ints) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Value> changes_val;
  Local<Value> last_insert_rowid_val;

  if (use_big_ints) {
    changes_val = BigInt::New(isolate, changes);
    last_insert_rowid_val = BigInt::New(isolate, last_insert_rowid);
  } else {
    changes_val = Number::New(isolate, changes);
    last_insert_rowid_val = Number::New(isolate, last_insert_rowid);
  }

  auto run_result_template = GetRunResultTemplate(env, isolate);

  MaybeLocal<Value> values[] = {changes_val, last_insert_rowid_val};
  Local<Object> result;
  if (!NewDictionaryInstance(context, run_result_template, values)
           .ToLocal(&result)) {
    return {};
  }

  return result;
}
}  // namespace

MaybeLocal<Object> StatementExecutionHelper::Run(Environment* env,
                                                 DatabaseSync* db,
                                                 sqlite3_stmt* stmt,
                                                 bool use_big_ints) {
  Isolate* isolate = env->isolate();
  EscapableHandleScope scope(isolate);
  sqlite3_step(stmt);
  int r = sqlite3_reset(stmt);
  CHECK_ERROR_OR_THROW(isolate, db, r, SQLITE_OK, MaybeLocal<Object>());

  sqlite3_int64 last_insert_rowid = sqlite3_last_insert_rowid(db->Connection());
  sqlite3_int64 changes = sqlite3_changes64(db->Connection());

  Local<Object> result;
  if (!CreateRunResultObject(env, changes, last_insert_rowid, use_big_ints)
           .ToLocal(&result)) {
    return MaybeLocal<Object>();
  }

  return scope.Escape(result);
}

BaseObjectPtr<StatementSyncIterator> StatementExecutionHelper::Iterate(
    Environment* env, BaseObjectPtr<StatementSync> stmt) {
  Local<Context> context = env->context();
  Local<Object> global = context->Global();
  Local<Value> js_iterator;
  Local<Value> js_iterator_prototype;
  if (!global->Get(context, env->iterator_string()).ToLocal(&js_iterator)) {
    return BaseObjectPtr<StatementSyncIterator>();
  }
  if (!js_iterator.As<Object>()
           ->Get(context, env->prototype_string())
           .ToLocal(&js_iterator_prototype)) {
    return BaseObjectPtr<StatementSyncIterator>();
  }

  BaseObjectPtr<StatementSyncIterator> iter =
      StatementSyncIterator::Create(env, stmt);

  if (!iter) {
    // Error in iterator creation, likely already threw in Create
    return BaseObjectPtr<StatementSyncIterator>();
  }

  if (iter->object()
          ->GetPrototypeV2()
          .As<Object>()
          ->SetPrototypeV2(context, js_iterator_prototype)
          .IsNothing()) {
    return BaseObjectPtr<StatementSyncIterator>();
  }

  return iter;
}

MaybeLocal<Value> StatementExecutionHelper::Get(Environment* env,
                                                DatabaseSync* db,
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

  LocalVector<Value> row_values(isolate);
  if (ExtractRowValues(env, stmt, num_cols, use_big_ints, &row_values)
          .IsNothing()) {
    return MaybeLocal<Value>();
  }

  if (return_arrays) {
    return scope.Escape(
        Array::New(isolate, row_values.data(), row_values.size()));
  } else {
    LocalVector<Name> keys(isolate);
    keys.reserve(num_cols);
    for (int i = 0; i < num_cols; ++i) {
      Local<Name> key;
      if (!ColumnNameToName(env, stmt, i).ToLocal(&key)) {
        return MaybeLocal<Value>();
      }
      keys.emplace_back(key);
    }

    DCHECK_EQ(keys.size(), row_values.size());
    return scope.Escape(Object::New(
        isolate, Null(isolate), keys.data(), row_values.data(), num_cols));
  }
}

void StatementSync::All(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  Isolate* isolate = env->isolate();
  int r = stmt->ResetStatement();
  CHECK_ERROR_OR_THROW(isolate, stmt->db_.get(), r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
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

void StatementSync::Iterate(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  int r = stmt->ResetStatement();
  CHECK_ERROR_OR_THROW(env->isolate(), stmt->db_.get(), r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
  }

  BaseObjectPtr<StatementSyncIterator> iter = StatementExecutionHelper::Iterate(
      env, BaseObjectPtr<StatementSync>(stmt));

  if (!iter) {
    return;
  }

  args.GetReturnValue().Set(iter->object());
}

void StatementSync::Get(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  int r = stmt->ResetStatement();
  CHECK_ERROR_OR_THROW(env->isolate(), stmt->db_.get(), r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
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

void StatementSync::Run(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_ON_BAD_STATE(
      env, stmt->IsFinalized(), "statement has been finalized");
  int r = stmt->ResetStatement();
  CHECK_ERROR_OR_THROW(env->isolate(), stmt->db_.get(), r, SQLITE_OK, void());

  if (!stmt->BindParams(args)) {
    return;
  }

  Local<Object> result;
  if (StatementExecutionHelper::Run(
          env, stmt->db_.get(), stmt->statement_, stmt->use_big_ints_)
          .ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

void StatementSync::Columns(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
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

void StatementSync::SetAllowUnknownNamedParameters(
    const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
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

void StatementSync::SetReturnArrays(const FunctionCallbackInfo<Value>& args) {
  StatementSync* stmt;
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
                         BaseObjectWeakPtr<DatabaseSync> database,
                         int capacity)
    : BaseObject(env, object),
      database_(std::move(database)),
      sql_tags_(capacity) {
  MakeWeak();
}

SQLTagStore::~SQLTagStore() {}

Local<FunctionTemplate> SQLTagStore::GetConstructorTemplate(Environment* env) {
  Local<FunctionTemplate> tmpl =
      env->sqlite_sql_tag_store_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "SQLTagStore"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        SQLTagStore::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "get", Get);
    SetProtoMethod(isolate, tmpl, "all", All);
    SetProtoMethod(isolate, tmpl, "iterate", Iterate);
    SetProtoMethod(isolate, tmpl, "run", Run);
    SetProtoMethod(isolate, tmpl, "clear", Clear);
    SetSideEffectFreeGetter(isolate,
                            tmpl,
                            FIXED_ONE_BYTE_STRING(isolate, "capacity"),
                            CapacityGetter);
    SetSideEffectFreeGetter(
        isolate, tmpl, FIXED_ONE_BYTE_STRING(isolate, "db"), DatabaseGetter);
    SetSideEffectFreeGetter(isolate, tmpl, env->size_string(), SizeGetter);
  }
  return tmpl;
}

BaseObjectPtr<SQLTagStore> SQLTagStore::Create(
    Environment* env, BaseObjectWeakPtr<DatabaseSync> database, int capacity) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }
  obj->SetInternalField(kDatabaseObject, database->object());
  return MakeBaseObject<SQLTagStore>(env, obj, std::move(database), capacity);
}

void SQLTagStore::CapacityGetter(const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* store;
  ASSIGN_OR_RETURN_UNWRAP(&store, args.This());
  args.GetReturnValue().Set(static_cast<double>(store->sql_tags_.Capacity()));
}

void SQLTagStore::DatabaseGetter(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
      args.This()->GetInternalField(kDatabaseObject).As<Value>());
}

void SQLTagStore::SizeGetter(const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* store;
  ASSIGN_OR_RETURN_UNWRAP(&store, args.This());
  args.GetReturnValue().Set(static_cast<double>(store->sql_tags_.Size()));
}

void SQLTagStore::Run(const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_ON_BAD_STATE(
      env, !session->database_->IsOpen(), "database is not open");

  BaseObjectPtr<StatementSync> stmt = PrepareStatement(args);

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

  Local<Object> result;
  if (StatementExecutionHelper::Run(
          env, stmt->db_.get(), stmt->statement_, stmt->use_big_ints_)
          .ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

void SQLTagStore::Iterate(const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.This());
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_ON_BAD_STATE(
      env, !session->database_->IsOpen(), "database is not open");

  BaseObjectPtr<StatementSync> stmt = PrepareStatement(args);

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

  BaseObjectPtr<StatementSyncIterator> iter = StatementExecutionHelper::Iterate(
      env, BaseObjectPtr<StatementSync>(stmt));

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

  BaseObjectPtr<StatementSync> stmt = PrepareStatement(args);

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

  BaseObjectPtr<StatementSync> stmt = PrepareStatement(args);

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

void SQLTagStore::Clear(const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* store;
  ASSIGN_OR_RETURN_UNWRAP(&store, args.This());
  store->sql_tags_.Clear();
}

BaseObjectPtr<StatementSync> SQLTagStore::PrepareStatement(
    const FunctionCallbackInfo<Value>& args) {
  SQLTagStore* session = BaseObject::FromJSObject<SQLTagStore>(args.This());
  if (!session) {
    THROW_ERR_INVALID_ARG_TYPE(
        Environment::GetCurrent(args)->isolate(),
        "This method can only be called on SQLTagStore instances.");
    return BaseObjectPtr<StatementSync>();
  }
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = isolate->GetCurrentContext();

  if (args.Length() < 1 || !args[0]->IsArray()) {
    THROW_ERR_INVALID_ARG_TYPE(
        isolate,
        "First argument must be an array of strings (template literal).");
    return BaseObjectPtr<StatementSync>();
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
      return BaseObjectPtr<StatementSync>();
    }
    Utf8Value part(isolate, str_val);
    sql += part.ToStringView();
    if (i < n_params) {
      sql += "?";
    }
  }

  BaseObjectPtr<StatementSync> stmt = nullptr;
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
        session->database_->connection_, sql.data(), sql.size(), &s, nullptr);

    if (r != SQLITE_OK) {
      THROW_ERR_SQLITE_ERROR(isolate, session->database_.get());
      sqlite3_finalize(s);
      return BaseObjectPtr<StatementSync>();
    }

    BaseObjectPtr<StatementSync> stmt_obj = StatementSync::Create(
        env, BaseObjectPtr<DatabaseSync>(session->database_), s);

    if (!stmt_obj) {
      THROW_ERR_SQLITE_ERROR(isolate, "Failed to create StatementSync");
      sqlite3_finalize(s);
      return BaseObjectPtr<StatementSync>();
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
    SetProtoMethod(isolate, tmpl, "iterate", StatementSync::Iterate);
    SetProtoMethod(isolate, tmpl, "all", StatementSync::All);
    SetProtoMethod(isolate, tmpl, "get", StatementSync::Get);
    SetProtoMethod(isolate, tmpl, "run", StatementSync::Run);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "columns", StatementSync::Columns);
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
    SetProtoMethod(isolate,
                   tmpl,
                   "setAllowUnknownNamedParameters",
                   StatementSync::SetAllowUnknownNamedParameters);
    SetProtoMethod(
        isolate, tmpl, "setReadBigInts", StatementSync::SetReadBigInts);
    SetProtoMethod(
        isolate, tmpl, "setReturnArrays", StatementSync::SetReturnArrays);
    env->set_sqlite_statement_sync_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<StatementSync> StatementSync::Create(
    Environment* env, BaseObjectPtr<DatabaseSync> db, sqlite3_stmt* stmt) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<StatementSync>(env, obj, std::move(db), stmt);
}

StatementSyncIterator::StatementSyncIterator(Environment* env,
                                             Local<Object> object,
                                             BaseObjectPtr<StatementSync> stmt)
    : BaseObject(env, object), stmt_(std::move(stmt)) {
  MakeWeak();
  done_ = false;
  statement_reset_generation_ = stmt_->reset_generation_;
}

StatementSyncIterator::~StatementSyncIterator() {}
void StatementSyncIterator::MemoryInfo(MemoryTracker* tracker) const {}

Local<FunctionTemplate> StatementSyncIterator::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl =
      env->sqlite_statement_sync_iterator_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "StatementSyncIterator"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        StatementSyncIterator::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "next", StatementSyncIterator::Next);
    SetProtoMethod(isolate, tmpl, "return", StatementSyncIterator::Return);
    env->set_sqlite_statement_sync_iterator_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<StatementSyncIterator> StatementSyncIterator::Create(
    Environment* env, BaseObjectPtr<StatementSync> stmt) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return BaseObjectPtr<StatementSyncIterator>();
  }

  return MakeBaseObject<StatementSyncIterator>(env, obj, std::move(stmt));
}

void StatementSyncIterator::Next(const FunctionCallbackInfo<Value>& args) {
  StatementSyncIterator* iter;
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

  THROW_AND_RETURN_ON_BAD_STATE(
      env,
      iter->statement_reset_generation_ != iter->stmt_->reset_generation_,
      "iterator was invalidated");

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

void StatementSyncIterator::Return(const FunctionCallbackInfo<Value>& args) {
  StatementSyncIterator* iter;
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

namespace {
namespace transfer {
struct null {};
struct boolean {
  bool value;
};
struct integer {
  sqlite3_int64 value;
  bool use_big_int = false;
};
struct real {
  double value;
};
struct text {
  std::string value;

  text() = default;
  explicit text(std::string&& str) : value(std::move(str)) {}
  explicit text(std::string_view str) : value(str.data(), str.size()) {}
  text(const char* str, size_t len) : value(str, len) {}
};
struct blob {
  std::vector<uint8_t> value;

  blob() = default;
  explicit blob(std::vector<uint8_t>&& vec) : value(std::move(vec)) {}
  explicit blob(std::span<const uint8_t> span)
      : value(span.begin(), span.end()) {}
  blob(const uint8_t* data, size_t len) : value(data, data + len) {}
};
using literal = std::variant<null, boolean, integer, real, text, blob>;
using value = std::variant<std::vector<literal>,
                           std::unordered_map<std::string, literal>,
                           literal>;

struct bind_literal {
  sqlite3_stmt* stmt;
  int index;

  int operator()(null) const { return sqlite3_bind_null(stmt, index); }
  int operator()(boolean b) const {
    return sqlite3_bind_int(stmt, index, b.value ? 1 : 0);
  }
  int operator()(integer i) const {
    return sqlite3_bind_int64(stmt, index, i.value);
  }
  int operator()(real r) const {
    return sqlite3_bind_double(stmt, index, r.value);
  }
  int operator()(const text& t) const {
    return sqlite3_bind_text64(stmt,
                               index,
                               t.value.data(),
                               static_cast<sqlite3_uint64>(t.value.size()),
                               SQLITE_STATIC,
                               SQLITE_UTF8);
  }
  int operator()(const blob& b) const {
    return sqlite3_bind_blob64(stmt,
                               index,
                               b.value.data(),
                               static_cast<sqlite3_uint64>(b.value.size()),
                               SQLITE_STATIC);
  }
  int operator()(const transfer::literal& literal) const {
    return std::visit(*this, literal);
  }
};

struct bind_value {
  sqlite3_stmt* stmt;

  int operator()(const literal&) const {
    // bind_value should only be called with vector or map
    return SQLITE_MISUSE;
  }
  int operator()(const std::vector<transfer::literal>& value) const {
    if (!std::in_range<int>(value.size())) [[unlikely]] {
      return SQLITE_RANGE;
    }
    bind_literal binder{stmt, static_cast<int>(value.size())};
    for (; binder.index > 0; --binder.index) {
      if (int r = binder(value[binder.index - 1]); r != SQLITE_OK) {
        return r;
      }
    }
    return SQLITE_OK;
  }
  int operator()(
      const std::unordered_map<std::string, transfer::literal>& value) const {
    bind_literal binder{stmt, 0};
    for (const auto& [name, value] : value) {
      binder.index = sqlite3_bind_parameter_index(stmt, name.c_str());
      if (binder.index == 0) {
        // No such named parameter, ignore this key.
        continue;
      }
      if (int r = binder(value); r != SQLITE_OK) {
        return r;
      }
    }
    return SQLITE_OK;
  }
  int operator()(const transfer::value& value) const {
    return std::visit(*this, value);
  }
};

literal FromColumn(sqlite3* db,
                   sqlite3_stmt* stmt,
                   const int col_index,
                   const bool use_big_ints) {
  int type = sqlite3_column_type(stmt, col_index);
  switch (type) {
    case SQLITE_NULL:
      return literal{in_place_type<null>};
    case SQLITE_INTEGER:
      return literal{in_place_type<integer>,
                     sqlite3_column_int64(stmt, col_index),
                     use_big_ints};
    case SQLITE_FLOAT:
      return literal{in_place_type<real>,
                     sqlite3_column_double(stmt, col_index)};
    case SQLITE_TEXT: {
      const char* text_value =
          reinterpret_cast<const char*>(sqlite3_column_text(stmt, col_index));
      CHECK_NOT_NULL(text_value);  // Catch OOM condition.
      int size = sqlite3_column_bytes(stmt, col_index);
      return literal{
          in_place_type<text>, text_value, static_cast<size_t>(size)};
    }
    case SQLITE_BLOB: {
      const uint8_t* blob_value = reinterpret_cast<const uint8_t*>(
          sqlite3_column_blob(stmt, col_index));
      if (blob_value == nullptr) [[unlikely]] {
        CHECK_NE(sqlite3_errcode(db), SQLITE_NOMEM);
        return literal{in_place_type<blob>};
      }
      int size = sqlite3_column_bytes(stmt, col_index);
      return literal{
          in_place_type<blob>, blob_value, static_cast<size_t>(size)};
    }
    default:
      UNREACHABLE("Bad SQLite value");
  }
}

value FromRow(sqlite3* connection,
              sqlite3_stmt* stmt,
              const int num_cols,
              const statement_options options) {
  if (options.return_arrays) {
    std::vector<transfer::literal> row;
    row.reserve(num_cols);
    for (int i = 0; i < num_cols; ++i) {
      row.push_back(
          transfer::FromColumn(connection, stmt, i, options.read_big_ints));
    }
    return row;
  } else {
    // TODO(BurningEnlightenment): share column names between rows
    // => return type should always be a vector of literals, and the caller
    //    should add an additional vector of column names if needed
    std::unordered_map<std::string, transfer::literal> row;
    for (int i = 0; i < num_cols; ++i) {
      const char* col_name = sqlite3_column_name(stmt, i);
      CHECK_NOT_NULL(col_name);  // Catch OOM condition.
      row.emplace(
          col_name,
          transfer::FromColumn(connection, stmt, i, options.read_big_ints));
    }
    return row;
  }
}

struct to_v8_value {
  Isolate* isolate;

  Local<Value> operator()(const null&) const { return Null(isolate); }
  Local<Value> operator()(const boolean& b) const {
    return Boolean::New(isolate, b.value);
  }
  Local<Value> operator()(const integer& i) const {
    constexpr auto kMaxSafeInteger = (sqlite3_int64{1} << 53) - 1;  // 2^53 - 1
    constexpr auto kMinSafeInteger = -kMaxSafeInteger;  // -(2^53 - 1)

    if (!i.use_big_int && kMinSafeInteger <= i.value &&
        i.value <= kMaxSafeInteger) {
      return Number::New(isolate, static_cast<double>(i.value));
    } else {
      return BigInt::New(isolate, i.value);
    }
  }
  Local<Value> operator()(const real& r) const {
    return Number::New(isolate, r.value);
  }
  Local<Value> operator()(const text& t) const {
    return String::NewFromUtf8(
               isolate, t.value.data(), NewStringType::kNormal, t.value.size())
        .ToLocalChecked();
  }
  Local<Value> operator()(const blob& b) const {
    const auto blobSize = b.value.size();
    auto buffer = ArrayBuffer::New(
        isolate, blobSize, BackingStoreInitializationMode::kUninitialized);
    std::memcpy(buffer->Data(), b.value.data(), blobSize);
    return Uint8Array::New(buffer, 0, blobSize);
  }
  Local<Value> operator()(const transfer::literal& literal) const {
    return std::visit(*this, literal);
  }
  Local<Value> operator()(const std::vector<transfer::literal>& vec) const {
    Local<Context> context = isolate->GetCurrentContext();
    Local<Array> array = Array::New(isolate, vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
      Local<Value> element = (*this)(vec[i]);
      // TODO(BurningEnlightenment): Proper bounds checking
      array->Set(context, static_cast<uint32_t>(i), element).Check();
    }
    return array;
  }
  Local<Value> operator()(
      const std::unordered_map<std::string, transfer::literal>& map) const {
    LocalVector<Name> obj_keys(isolate);
    LocalVector<Value> obj_values(isolate);
    for (const auto& [key, value] : map) {
      // TODO(BurningEnlightenment): Crash on OOM ok?
      obj_keys.emplace_back(
          String::NewFromUtf8(
              isolate, key.data(), NewStringType::kNormal, key.size())
              .ToLocalChecked());
      obj_values.emplace_back((*this)(value));
    }
    return Object::New(
        isolate, Null(isolate), obj_keys.data(), obj_values.data(), map.size());
  }
  Local<Value> operator()(const transfer::value& value) const {
    return std::visit(*this, value);
  }
};

Maybe<transfer::literal> ToLiteral(Isolate* isolate, Local<Value> value) {
  if (value->IsNumber()) {
    return v8::Just(literal{in_place_type<real>, value.As<Number>()->Value()});
  } else if (value->IsString()) {
    Utf8Value utf8_value(isolate, value.As<String>());
    return v8::Just(
        literal{in_place_type<text>, *utf8_value, utf8_value.length()});
  } else if (value->IsNull()) {
    return v8::Just(literal{in_place_type<null>});
  } else if (value->IsBigInt()) {
    bool lossless{};
    sqlite3_int64 int_value = value.As<BigInt>()->Int64Value(&lossless);
    if (!lossless) {
      THROW_ERR_INVALID_ARG_VALUE(isolate,
                                  "BigInt value is too large to bind.");
      return {};
    }
    return v8::Just(literal{in_place_type<integer>, int_value});
  } else if (value->IsArrayBufferView()) {
    ArrayBufferViewContents<uint8_t> buf(value);
    return v8::Just(literal{in_place_type<blob>, buf.data(), buf.length()});
  } else [[unlikely]] {
    THROW_ERR_INVALID_ARG_TYPE(
        isolate, "Provided value cannot be bound to SQLite parameter.");
    return {};
  }
}
Maybe<transfer::value> ToValue(Isolate* isolate, Local<Object> object) {
  Local<Array> property_names;
  if (!object->GetOwnPropertyNames(isolate->GetCurrentContext())
           .ToLocal(&property_names)) [[unlikely]] {
    return {};
  }
  const uint32_t length = property_names->Length();
  Local<Context> context = isolate->GetCurrentContext();
  std::unordered_map<std::string, literal> map;
  map.reserve(length);
  for (uint32_t i = 0; i < length; ++i) {
    Local<Value> key;
    if (!property_names->Get(context, i).ToLocal(&key) || !key->IsString())
        [[unlikely]] {
      THROW_ERR_INVALID_ARG_TYPE(
          isolate,
          "Object keys must be strings to morph to SQLite parameters.");
      return {};
    }
    Utf8Value utf8_key(isolate, key.As<String>());
    Local<Value> value;
    if (!object->Get(context, key).ToLocal(&value)) [[unlikely]] {
      return {};
    }
    auto maybe_literal = ToLiteral(isolate, value);
    if (maybe_literal.IsNothing()) [[unlikely]] {
      return {};
    }
    map.emplace(utf8_key.operator*(), std::move(maybe_literal).FromJust());
  }
  return v8::Just(value{in_place_type<decltype(map)>, std::move(map)});
}
Maybe<transfer::value> ToValue(Isolate* isolate, Local<Array> array) {
  const uint32_t length = array->Length();
  Local<Context> context = isolate->GetCurrentContext();
  std::vector<literal> vec;
  vec.reserve(length);
  for (uint32_t i = 0; i < length; ++i) {
    Local<Value> value;
    if (!array->Get(context, i).ToLocal(&value)) [[unlikely]] {
      return {};
    }
    auto maybe_literal = ToLiteral(isolate, value);
    if (maybe_literal.IsNothing()) [[unlikely]] {
      return {};
    }
    vec.push_back(std::move(maybe_literal).FromJust());
  }
  return v8::Just(value{in_place_type<decltype(vec)>, std::move(vec)});
}
Maybe<transfer::value> ToValue(Isolate* isolate, Local<Value> value) {
  if (value->IsArray()) {
    return ToValue(isolate, value.As<Array>());
  } else if (value->IsObject()) {
    return ToValue(isolate, value.As<Object>());
  } else [[unlikely]] {
    THROW_ERR_INVALID_ARG_TYPE(
        isolate,
        "Value must be an object or array to morph to SQLite parameters.");
    return {};
  }
}
}  // namespace transfer

class OperationBase {
 public:
  Local<Promise::Resolver> GetResolver(Isolate* isolate) const {
    return resolver_.Get(isolate);
  }

 protected:
  explicit OperationBase(Global<Promise::Resolver> resolver)
      : resolver_(std::move(resolver)) {}

  Global<Promise::Resolver> resolver_;
};

class alignas(64) OperationResult {
  class Rejected {
   public:
    static Rejected ErrorCode(int error_code,
                              const char* error_message = nullptr) {
      const char* error_description = sqlite3_errstr(error_code);
      return Rejected{
          error_code,
          error_message != nullptr ? error_message : std::string{},
          error_description != nullptr ? error_description : std::string{},
      };
    }
    static Rejected LastError(sqlite3* connection) {
      int error_code = sqlite3_extended_errcode(connection);
      const char* error_message = sqlite3_errmsg(connection);
      return Rejected::ErrorCode(error_code, error_message);
    }

    void Connect(Environment* env,
                 Local<Context> context,
                 Promise::Resolver* resolver) const {
      Isolate* isolate = env->isolate();

      // Use ToLocalChecked here because we are trying to report a failure and
      // failing to report a failure would be worse than crashing as it would be
      // an API contract violation.
      Local<String> error_message =
          String::NewFromUtf8(isolate,
                              error_message_.data(),
                              NewStringType::kNormal,
                              static_cast<int>(std::ssize(error_message_)))
              .ToLocalChecked();
      Local<String> error_description =
          String::NewFromUtf8(isolate,
                              error_description_.data(),
                              NewStringType::kInternalized,
                              static_cast<int>(std::ssize(error_description_)))
              .ToLocalChecked();

      Local<Object> error =
          Exception::Error(error_message)->ToObject(context).ToLocalChecked();
      error
          ->Set(context,
                FIXED_ONE_BYTE_STRING(isolate, "code"),
                FIXED_ONE_BYTE_STRING(isolate, "ERR_SQLITE_ERROR"))
          .Check();
      error
          ->Set(context,
                FIXED_ONE_BYTE_STRING(isolate, "errcode"),
                Integer::New(isolate, error_code_))
          .Check();
      error
          ->Set(context,
                FIXED_ONE_BYTE_STRING(isolate, "errstr"),
                error_description)
          .Check();

      resolver->Reject(context, error).Check();
    }

   private:
    Rejected(int error_code,
             std::string error_message,
             std::string error_description)
        : error_message_(std::move(error_message)),
          error_description_(std::move(error_description)),
          error_code_(error_code) {}

    std::string error_message_;
    std::string error_description_;
    int error_code_;
  };
  class Void {
   public:
    void Connect(Environment* env,
                 Local<Context> context,
                 Promise::Resolver* resolver) const {
      resolver->Resolve(context, Undefined(env->isolate())).Check();
    }
  };
  class PreparedStatement {
   public:
    explicit PreparedStatement(BaseObjectPtr<Database> db,
                               sqlite3_stmt* stmt,
                               statement_options options)
        : db_(std::move(db)), stmt_(stmt), options_(options) {}

    void Connect(Environment* env,
                 Local<Context> context,
                 Promise::Resolver* resolver) const {
      Isolate* isolate = env->isolate();
      auto* stmt = stmt_;
      if (!db_->IsOpen()) {
        // Database is closing, therefore directly create a disposed Statement.
        (void)sqlite3_finalize(stmt);
        stmt = nullptr;
      }
      auto stmt_obj = Statement::Create(
          Environment::GetCurrent(context), db_, stmt, options_);
      if (stmt_obj) {
        resolver->Resolve(context, stmt_obj->object()).Check();
      } else {
        Local<String> error_message =
            String::NewFromUtf8(isolate,
                                "Failed to create Statement object",
                                NewStringType::kNormal)
                .ToLocalChecked();
        Local<Object> error =
            Exception::Error(error_message)->ToObject(context).ToLocalChecked();
        resolver->Reject(context, error).Check();
      }
    }

   private:
    BaseObjectPtr<Database> db_;
    sqlite3_stmt* stmt_;
    statement_options options_;
  };
  class Value {
   public:
    explicit Value(transfer::value value) : value_(std::move(value)) {}

    void Connect(Environment* env,
                 Local<Context> context,
                 Promise::Resolver* resolver) const {
      resolver->Resolve(context, transfer::to_v8_value(env->isolate())(value_))
          .Check();
    }

   private:
    transfer::value value_;
  };
  class Values {
   public:
    explicit Values(std::vector<transfer::value> values)
        : values_(std::move(values)) {}

    void Connect(Environment* env,
                 Local<Context> context,
                 Promise::Resolver* resolver) const {
      Isolate* isolate = env->isolate();
      CHECK_LE(values_.size(),
               static_cast<size_t>(std::numeric_limits<int>::max()));
      int size = static_cast<int>(values_.size());
      Local<Array> array = Array::New(isolate, size);
      for (int i = 0; i < size; ++i) {
        auto element = transfer::to_v8_value(isolate)(values_[i]);
        array->Set(context, static_cast<uint32_t>(i), element).Check();
      }
      resolver->Resolve(context, array).Check();
    }

   private:
    std::vector<transfer::value> values_;
  };
  class RunResult {
   public:
    RunResult(sqlite3_int64 changes,
              sqlite3_int64 last_insert_rowid,
              bool use_big_ints)
        : changes_(changes),
          last_insert_rowid_(last_insert_rowid),
          use_big_ints_(use_big_ints) {}

    void Connect(Environment* env,
                 Local<Context> context,
                 Promise::Resolver* resolver) const {
      auto result = CreateRunResultObject(
                        env, changes_, last_insert_rowid_, use_big_ints_)
                        .ToLocalChecked();
      resolver->Resolve(context, result).Check();
    }

   private:
    sqlite3_int64 changes_;
    sqlite3_int64 last_insert_rowid_;
    bool use_big_ints_;
  };

  using variant_type =
      std::variant<Void, Value, Values, Rejected, PreparedStatement, RunResult>;

 public:
  static OperationResult RejectErrorCode(OperationBase* origin,
                                         int error_code,
                                         const char* error_message = nullptr) {
    return OperationResult{origin,
                           Rejected::ErrorCode(error_code, error_message)};
  }
  static OperationResult RejectLastError(OperationBase* origin,
                                         sqlite3* connection) {
    return OperationResult{origin, Rejected::LastError(connection)};
  }
  static OperationResult ResolveVoid(OperationBase* origin) {
    return OperationResult{origin, Void{}};
  }
  static OperationResult ResolveValue(OperationBase* origin,
                                      transfer::value&& value) {
    return OperationResult{origin, Value{std::move(value)}};
  }
  static OperationResult ResolveValues(OperationBase* origin,
                                       std::vector<transfer::value>&& values) {
    return OperationResult{origin, Values{std::move(values)}};
  }
  static OperationResult ResolveRunResult(OperationBase* origin,
                                          sqlite3_int64 changes,
                                          sqlite3_int64 last_insert_rowid,
                                          bool use_big_ints) {
    return OperationResult{origin,
                           RunResult{changes, last_insert_rowid, use_big_ints}};
  }
  static OperationResult ResolvePreparedStatement(OperationBase* origin,
                                                  BaseObjectPtr<Database> db,
                                                  sqlite3_stmt* stmt,
                                                  statement_options options) {
    return OperationResult{origin,
                           PreparedStatement{std::move(db), stmt, options}};
  }

  template <typename T>
    requires std::constructible_from<variant_type, T>
  OperationResult(OperationBase* origin, T&& value)
      : origin_(origin), result_(std::forward<T>(value)) {}

  void Connect(Environment* env) const {
    Local<Context> context = env->context();
    Local<Promise::Resolver> resolver = origin_->GetResolver(env->isolate());
    std::visit([env, &context, &resolver](
                   auto&& value) { value.Connect(env, context, *resolver); },
               result_);
  }

 private:
  OperationBase* origin_ = nullptr;
  variant_type result_;
};

class PrepareStatementOperation : private OperationBase {
 public:
  PrepareStatementOperation(Global<Promise::Resolver>&& resolver,
                            BaseObjectPtr<Database>&& db,
                            std::string&& sql,
                            statement_options options = {})
      : OperationBase(std::move(resolver)),
        db_(std::move(db)),
        sql_(std::move(sql)),
        options_(options) {}

  OperationResult operator()(sqlite3* connection) {
    sqlite3_stmt* stmt = nullptr;
    int error_code =
        sqlite3_prepare_v2(connection, sql_.c_str(), -1, &stmt, nullptr);
    return error_code == SQLITE_OK
               ? OperationResult::ResolvePreparedStatement(
                     this, std::move(db_), stmt, options_)
               : OperationResult::RejectLastError(this, connection);
  }

 private:
  BaseObjectPtr<Database> db_;
  std::string sql_;
  statement_options options_;
};

class FinalizeStatementOperation : private OperationBase {
 public:
  FinalizeStatementOperation(Global<Promise::Resolver>&& resolver,
                             sqlite3_stmt* stmt)
      : OperationBase(std::move(resolver)), stmt_(stmt) {}

  OperationResult operator()(sqlite3* connection) {
    int error_code = sqlite3_finalize(stmt_);
    CHECK_NE(error_code, SQLITE_MISUSE);
    stmt_ = nullptr;
    return OperationResult::ResolveVoid(this);
  }

 private:
  sqlite3_stmt* stmt_;
};

class StatementGetOperation : private OperationBase {
 public:
  StatementGetOperation(Global<Promise::Resolver>&& resolver,
                        sqlite3_stmt* stmt,
                        transfer::value&& bind_arguments,
                        statement_options options)
      : OperationBase(std::move(resolver)),
        stmt_(stmt),
        bind_arguments_(std::move(bind_arguments)),
        options_(options) {}

  OperationResult operator()(sqlite3* connection) {
    auto clear_bindings = OnScopeLeave([&] { sqlite3_clear_bindings(stmt_); });
    if (int r = transfer::bind_value(stmt_)(bind_arguments_); r != SQLITE_OK) {
      return OperationResult::RejectErrorCode(this, r);
    }
    auto reset_statement = OnScopeLeave([&] { sqlite3_reset(stmt_); });
    int error_code = sqlite3_step(stmt_);
    if (error_code == SQLITE_DONE) {
      return OperationResult::ResolveVoid(this);
    }
    if (error_code != SQLITE_ROW) {
      return OperationResult::RejectLastError(this, connection);
    }

    int num_cols = sqlite3_column_count(stmt_);
    if (num_cols == 0) {
      return OperationResult::ResolveVoid(this);
    }

    return OperationResult::ResolveValue(
        this, transfer::FromRow(connection, stmt_, num_cols, options_));
  }

 private:
  sqlite3_stmt* stmt_;
  transfer::value bind_arguments_;
  statement_options options_;
};

class StatementAllOperation : private OperationBase {
 public:
  StatementAllOperation(Global<Promise::Resolver>&& resolver,
                        sqlite3_stmt* stmt,
                        transfer::value&& bind_arguments,
                        statement_options options)
      : OperationBase(std::move(resolver)),
        stmt_(stmt),
        bind_arguments_(std::move(bind_arguments)),
        options_(options) {}

  OperationResult operator()(sqlite3* connection) {
    auto clear_bindings = OnScopeLeave([&] { sqlite3_clear_bindings(stmt_); });
    if (int r = transfer::bind_value(stmt_)(bind_arguments_); r != SQLITE_OK) {
      return OperationResult::RejectErrorCode(this, r);
    }
    auto reset_statement = OnScopeLeave([&] { sqlite3_reset(stmt_); });

    std::vector<transfer::value> rows;
    int r;
    int num_cols = sqlite3_column_count(stmt_);
    for (r = sqlite3_step(stmt_); r == SQLITE_ROW; r = sqlite3_step(stmt_)) {
      rows.emplace_back(
          transfer::FromRow(connection, stmt_, num_cols, options_));
    }
    return OperationResult::ResolveValues(this, std::move(rows));
  }

 private:
  sqlite3_stmt* stmt_;
  transfer::value bind_arguments_;
  statement_options options_;
};

class StatementRunOperation : private OperationBase {
 public:
  StatementRunOperation(Global<Promise::Resolver>&& resolver,
                        sqlite3_stmt* stmt,
                        transfer::value&& bind_arguments,
                        bool use_big_ints)
      : OperationBase(std::move(resolver)),
        stmt_(stmt),
        bind_arguments_(std::move(bind_arguments)),
        use_big_ints_(use_big_ints) {}

  OperationResult operator()(sqlite3* connection) {
    auto clear_bindings = OnScopeLeave([&] { sqlite3_clear_bindings(stmt_); });
    if (int r = transfer::bind_value(stmt_)(bind_arguments_); r != SQLITE_OK) {
      return OperationResult::RejectErrorCode(this, r);
    }
    (void)sqlite3_step(stmt_);
    if (sqlite3_reset(stmt_) != SQLITE_OK) {
      return OperationResult::RejectLastError(this, connection);
    }

    sqlite3_int64 last_insert_rowid = sqlite3_last_insert_rowid(connection);
    sqlite3_int64 changes = sqlite3_changes(connection);
    return OperationResult::ResolveRunResult(
        this, changes, last_insert_rowid, use_big_ints_);
  }

 private:
  sqlite3_stmt* stmt_;
  transfer::value bind_arguments_;
  bool use_big_ints_;
};

class ExecOperation : private OperationBase {
 public:
  ExecOperation(Global<Promise::Resolver>&& resolver, std::string&& sql)
      : OperationBase(std::move(resolver)), sql_(std::move(sql)) {}

  OperationResult operator()(sqlite3* connection) {
    int error_code =
        sqlite3_exec(connection, sql_.c_str(), nullptr, nullptr, nullptr);
    return error_code == SQLITE_OK
               ? OperationResult::ResolveVoid(this)
               : OperationResult::RejectLastError(this, connection);
  }

 private:
  std::string sql_;
};

class CloseOperation : private OperationBase {
 public:
  explicit CloseOperation(Global<Promise::Resolver>&& resolver)
      : OperationBase(std::move(resolver)) {}

  OperationResult operator()(sqlite3* connection) {
    // TODO(BurningEnlightenment): close the associated statements
    int error_code = sqlite3_close(connection);
    // Busy means that we failed to cleanup associated statements, i.e. we
    // failed to maintain the Database object invariants.
    CHECK_NE(error_code, SQLITE_BUSY);
    DCHECK_NE(error_code, SQLITE_MISUSE);
    return error_code == SQLITE_OK
               ? OperationResult::ResolveVoid(this)
               : OperationResult::RejectErrorCode(this, error_code);
  }
};

class IsInTransactionOperation : private OperationBase {
 public:
  explicit IsInTransactionOperation(Global<Promise::Resolver>&& resolver)
      : OperationBase(std::move(resolver)) {}

  OperationResult operator()(sqlite3* connection) {
    transfer::boolean in_transaction{sqlite3_get_autocommit(connection) == 0};
    return OperationResult::ResolveValue(this, transfer::value{in_transaction});
  }
};

class LocationOperation : private OperationBase {
 public:
  LocationOperation(Global<Promise::Resolver>&& resolver, std::string&& db_name)
      : OperationBase(std::move(resolver)), db_name_(std::move(db_name)) {}

  OperationResult operator()(sqlite3* connection) {
    const char* location = sqlite3_db_filename(connection, db_name_.c_str());
    transfer::literal location_literal{transfer::null{}};
    if (location && location[0] != '\0') {
      location_literal = transfer::text{location, std::strlen(location)};
    }
    return OperationResult::ResolveValue(
        this, transfer::value{std::move(location_literal)});
  }

 private:
  std::string db_name_;
};

class UpdateDbConfigOperation : private OperationBase {
 public:
  UpdateDbConfigOperation(Global<Promise::Resolver>&& resolver,
                          int db_config,
                          int value)
      : OperationBase(std::move(resolver)),
        db_config_(db_config),
        value_(value) {}

  OperationResult operator()(sqlite3* connection) {
    int error_code = sqlite3_db_config(connection, db_config_, value_, nullptr);
    return error_code == SQLITE_OK
               ? OperationResult::ResolveVoid(this)
               : OperationResult::RejectLastError(this, connection);
  }

 private:
  int db_config_;
  int value_;
};

class LoadExtensionOperation : private OperationBase {
 public:
  LoadExtensionOperation(Global<Promise::Resolver>&& resolver,
                         std::string&& extension_path)
      : OperationBase(std::move(resolver)),
        extension_path_(std::move(extension_path)) {}

  OperationResult operator()(sqlite3* connection) {
    char* error_message = nullptr;
    auto free_error_message =
        OnScopeLeave([&] { sqlite3_free(error_message); });
    int error_code = sqlite3_load_extension(
        connection, extension_path_.c_str(), nullptr, &error_message);
    return error_code == SQLITE_OK ? OperationResult::ResolveVoid(this)
                                   : OperationResult::RejectErrorCode(
                                         this, error_code, error_message);
  }

 private:
  std::string extension_path_;
};

using Operation = std::variant<ExecOperation,
                               StatementGetOperation,
                               StatementAllOperation,
                               StatementRunOperation,
                               PrepareStatementOperation,
                               FinalizeStatementOperation,
                               IsInTransactionOperation,
                               LocationOperation,
                               UpdateDbConfigOperation,
                               LoadExtensionOperation,
                               CloseOperation>;

template <typename T, typename V>
struct is_contained_in_variant;
template <typename T, typename... Args>
struct is_contained_in_variant<T, std::variant<Args...>> {
  static constexpr bool value{(std::is_same_v<T, Args> || ...)};
};

template <typename Op>
inline constexpr bool is_operation_type =
    is_contained_in_variant<Op, Operation>::value;

enum class QueuePushResult {
  kQueueFull = -1,
  kSuccess,
  kLastSlot,
};
}  // namespace

class DatabaseOperationQueue {
 public:
  explicit DatabaseOperationQueue(size_t capacity) : operations_(), results_() {
    operations_.reserve(capacity);
    results_.reserve(capacity);
  }

  [[nodiscard]] QueuePushResult Push(Operation operation) {
    if (operations_.capacity() == operations_.size()) {
      return QueuePushResult::kQueueFull;
    }
    operations_.push_back(std::move(operation));
    return operations_.size() == operations_.capacity()
               ? QueuePushResult::kLastSlot
               : QueuePushResult::kSuccess;
  }
  template <typename Op, typename... Args>
    requires is_operation_type<Op> &&
             std::constructible_from<Operation, std::in_place_type_t<Op>,
                                     Global<Promise::Resolver>&&,
                                     Args&&...> [[nodiscard]] QueuePushResult
    PushEmplace(Isolate* isolate,
                Local<Promise::Resolver> resolver,
                Args&&... args) {
    return Push(Operation{std::in_place_type<Op>,
                          Global<Promise::Resolver>{isolate, resolver},
                          std::forward<Args>(args)...});
  }
  void Push(OperationResult result) {
    Mutex::ScopedLock lock{results_mutex_};
    CHECK_LT(results_.size(), results_.capacity());
    results_.push_back(std::move(result));
  }

  Operation* PopOperation() {
    if (operations_.size() == pending_index_) {
      return nullptr;
    }
    return &operations_[pending_index_++];
  }
  std::span<OperationResult> PopResults() {
    Mutex::ScopedLock lock{results_mutex_};
    if (results_.size() == completed_index_) {
      return {};
    }
    return std::span{results_}.subspan(
        std::exchange(completed_index_, results_.size()));
  }

 private:
  std::vector<Operation> operations_;
  std::vector<OperationResult> results_;
  size_t pending_index_ = 0;
  size_t completed_index_ = 0;
  Mutex results_mutex_;
};

namespace {
using uv_async_deleter = decltype([](uv_async_t* handle) -> void {
  uv_close(reinterpret_cast<uv_handle_t*>(handle),
           [](uv_handle_t* handle) -> void {
             delete reinterpret_cast<uv_async_t*>(handle);
           });
});
using unique_async_handle = std::unique_ptr<uv_async_t, uv_async_deleter>;
unique_async_handle make_unique_async_handle(uv_loop_t* loop,
                                             uv_async_cb callback) {
  auto* handle = new uv_async_t;
  if (uv_async_init(loop, handle, callback) != 0) {
    delete handle;
    return nullptr;
  }
  return unique_async_handle{handle};
}
}  // namespace

class DatabaseOperationExecutor final : private ThreadPoolWork {
 public:
  DatabaseOperationExecutor(Environment* env, sqlite3* connection)
      : ThreadPoolWork(env, "node_sqlite3.OperationExecutor"),
        async_completion_(),
        connection_(connection) {}

  bool is_working() const { return current_batch_ != nullptr; }

  void ScheduleBatch(std::unique_ptr<DatabaseOperationQueue> batch) {
    CHECK_NOT_NULL(batch);
    auto batch_ptr = batch.get();
    batches_.push(std::move(batch));
    if (!is_working()) {
      async_completion_ = make_unique_async_handle(env()->event_loop(),
                                                   &AsyncCompletionCallback);
      async_completion_->data = this;
      DatabaseOperationExecutor::ScheduleWork(batch_ptr);
    }
  }

  void Dispose() {
    CHECK(is_working());
    disposing_ = true;
  }

 private:
  void DoThreadPoolWork() override {
    // Use local copies of member variables to avoid false sharing.
    sqlite3* connection = connection_;
    DatabaseOperationQueue* batch = current_batch_;
    for (Operation* maybe_op = batch->PopOperation(); maybe_op != nullptr;
         maybe_op = batch->PopOperation()) {
      auto op_result = std::visit<OperationResult>(
          [connection](auto&& op) -> OperationResult { return op(connection); },
          *maybe_op);
      batch->Push(std::move(op_result));
      CHECK_EQ(uv_async_send(async_completion_.get()), 0);
    }
  }
  void AfterThreadPoolWork(int status) override {
    CHECK_EQ(status, 0);  // Currently the cancellation API is not exposed.
    AsyncCompletionCallback();
    current_batch_ = nullptr;
    batches_.pop();
    if (!batches_.empty()) {
      DatabaseOperationExecutor::ScheduleWork(batches_.front().get());
    } else if (disposing_) {
      // The executor is being disposed and there are no more batches to
      // process, so we can safely delete it.
      delete this;
    } else {
      // Don't keep the event loop active while there are no batches to process.
      async_completion_.reset();
    }
  }
  static void AsyncCompletionCallback(uv_async_t* handle) {
    if (uv_is_closing(reinterpret_cast<uv_handle_t*>(handle))) {
      return;
    }
    auto* self = static_cast<DatabaseOperationExecutor*>(handle->data);
    // Safe guard against a race between the async_completion and the after_work
    // callback; given the current libuv implementation this should not be
    // possible, but it's not a documented API guarantee.
    if (self->current_batch_ != nullptr) {
      self->AsyncCompletionCallback();
    }
  }
  void AsyncCompletionCallback() {
    const auto& results = current_batch_->PopResults();
    if (results.empty()) {
      return;
    }
    Environment* env = this->env();
    HandleScope handle_scope(env->isolate());

    for (OperationResult& result : results) {
      result.Connect(env);
    }
  }
  void ScheduleWork(DatabaseOperationQueue* batch) {
    CHECK_NOT_NULL(batch);
    CHECK_NULL(current_batch_);
    current_batch_ = batch;
    ThreadPoolWork::ScheduleWork();
  }

  std::queue<std::unique_ptr<DatabaseOperationQueue>> batches_;
  bool disposing_ = false;

  // These _must_ only be written to while is_working() is false or from within
  // AfterThreadPoolWork. Violating this invariant would introduce to data races
  // and therefore undefined behavior.
  unique_async_handle async_completion_;
  sqlite3* connection_ = nullptr;
  DatabaseOperationQueue* current_batch_ = nullptr;
};

void Database::PrepareNextBatch() {
  CHECK_NULL(next_batch_);
  next_batch_ = std::make_unique<DatabaseOperationQueue>(kDefaultBatchSize);

  // TODO(BurningEnlightenment): Do I need to retain a BaseObjectPtr?
  env()->isolate()->EnqueueMicrotask(
      [](void* self) -> void {
        CHECK_NOT_NULL(self);
        static_cast<Database*>(self)->ProcessNextBatch();
      },
      this);
}
void Database::ProcessNextBatch() {
  if (next_batch_ != nullptr) {
    executor_->ScheduleBatch(std::move(next_batch_));
  }
}
template <typename Op, typename... Args>
void Database::Schedule(v8::Isolate* isolate,
                        v8::Local<v8::Promise::Resolver> resolver,
                        Args&&... args) {
  if (next_batch_ == nullptr) {
    PrepareNextBatch();
  }
  QueuePushResult r = next_batch_->PushEmplace<Op>(
      isolate, resolver, std::forward<Args>(args)...);
  if (r == QueuePushResult::kSuccess) [[likely]] {
    return;
  }
  // Batch is full, schedule it for execution.
  ProcessNextBatch();
  // With the current set of invariants next_batch_ should never be full
  CHECK_EQ(r, QueuePushResult::kLastSlot);
}

template <typename Op, typename... Args>
Local<Promise> Database::Schedule(Args&&... args) {
  Isolate* isolate = env()->isolate();
  Local<Context> context = isolate->GetCurrentContext();
  auto resolver = Promise::Resolver::New(context).ToLocalChecked();

  Database::Schedule<Op>(isolate, resolver, std::forward<Args>(args)...);

  return resolver->GetPromise();
}

Database::Database(Environment* env,
                   v8::Local<v8::Object> object,
                   DatabaseOpenConfiguration&& open_config,
                   bool open,
                   bool allow_load_extension)
    : DatabaseCommon(env, object, std::move(open_config), allow_load_extension),
      executor_(nullptr),
      next_batch_(nullptr) {
  MakeWeak();

  if (open) {
    Database::Open();
  }
}

Database::~Database() {
  if (!IsOpen()) [[likely]] {
    return;
  }
  if (executor_->is_working()) {
    HandleScope handle_scope(env()->isolate());
    (void)AsyncDisposeImpl();
  } else {
    (void)sqlite3_close_v2(connection_);
  }

  env()->SetImmediate([](Environment* env) {
    HandleScope handle_scope(env->isolate());

    THROW_ERR_INVALID_STATE(
        env->isolate(),
        "Database was not properly closed before being garbage collected. "
        "Please call and await db.close() to close the database connection.");
  });
}

void Database::MemoryInfo(MemoryTracker* tracker) const {
  // TODO(BurningEnlightenment): more accurately track the size of all fields
  tracker->TrackFieldWithSize(
      "open_config", sizeof(open_config_), "DatabaseOpenConfiguration");
}

namespace {
v8::Local<v8::FunctionTemplate> CreateDatabaseConstructorTemplate(
    Environment* env) {
  Isolate* isolate = env->isolate();

  Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, Database::New);
  tmpl->InstanceTemplate()->SetInternalFieldCount(
      Database::kInternalFieldCount);

  AddDatabaseCommonMethodsToTemplate(isolate, tmpl);

  SetProtoMethod(isolate, tmpl, "close", Database::Close);
  SetProtoAsyncDispose(isolate, tmpl, Database::AsyncDispose);
  SetProtoMethod(isolate, tmpl, "prepare", Database::Prepare);
  SetProtoMethod(isolate, tmpl, "exec", Database::Exec);
  SetProtoMethod(isolate, tmpl, "isInTransaction", Database::IsInTransaction);
  SetProtoMethod(isolate, tmpl, "location", Database::Location);
  SetProtoMethod(isolate, tmpl, "enableDefensive", Database::EnableDefensive);
  SetProtoMethod(
      isolate, tmpl, "enableLoadExtension", Database::EnableLoadExtension);
  SetProtoMethod(isolate, tmpl, "loadExtension", Database::LoadExtension);

  Local<String> sqlite_type_key = FIXED_ONE_BYTE_STRING(isolate, "sqlite-type");
  Local<v8::Symbol> sqlite_type_symbol =
      v8::Symbol::For(isolate, sqlite_type_key);
  Local<String> database_sync_string =
      FIXED_ONE_BYTE_STRING(isolate, "node:sqlite-async");
  tmpl->InstanceTemplate()->Set(sqlite_type_symbol, database_sync_string);

  return tmpl;
}
}  // namespace

void Database::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
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
    if (!ParseCommonDatabaseOptions(
            env, options, open_config, open, allow_load_extension)) {
      return;
    }
  }

  new Database(
      env, args.This(), std::move(open_config), open, allow_load_extension);
}

bool Database::Open() {
  if (IsDisposed()) {
    THROW_ERR_INVALID_STATE(env(), "database is disposed");
    return false;
  }
  auto open_result = DatabaseCommon::Open();
  if (open_result) {
    executor_ = std::make_unique<DatabaseOperationExecutor>(env(), connection_);
  }
  return open_result;
}

bool Database::IsDisposed() const {
  return object()
      ->GetInternalField(Database::kDisposePromiseSlot)
      .As<Value>()
      ->IsPromise();
}

v8::Local<v8::Promise> Database::EnterDisposedStateSync() {
  Isolate* isolate = env()->isolate();
  Local<Context> context = isolate->GetCurrentContext();
  auto disposed = Promise::Resolver::New(context).ToLocalChecked();
  disposed->Resolve(context, Undefined(isolate)).Check();

  auto dispose_promise = disposed->GetPromise();
  object()->SetInternalField(Database::kDisposePromiseSlot, dispose_promise);
  return dispose_promise;
}

Local<Promise> Database::AsyncDisposeImpl() {
  Isolate* isolate = env()->isolate();
  Local<Context> context = isolate->GetCurrentContext();
  auto resolver = Promise::Resolver::New(context).ToLocalChecked();

  // We can't use Schedule here, because Schedule queues a MicroTask which
  // would try to access the Database object after destruction if this is
  // called from the destructor.
  // Furthermore we always need to schedule the CloseOperation in a separate
  // batch, to ensure that it runs after all previously scheduled operations,
  // because e.g. PrepareStatementOperations need to connect their results
  // first.
  ProcessNextBatch();
  next_batch_ = std::make_unique<DatabaseOperationQueue>(1U);
  CHECK_NE(next_batch_->PushEmplace<CloseOperation>(isolate, resolver),
           QueuePushResult::kQueueFull);
  ProcessNextBatch();
  executor_.release()->Dispose();

  connection_ = nullptr;

  auto dispose_promise = resolver->GetPromise();
  object()->SetInternalField(Database::kDisposePromiseSlot, dispose_promise);
  return dispose_promise;
}

void Database::Close(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());

  Environment* env = Environment::GetCurrent(args);
  if (!db->IsOpen()) {
    if (!db->IsDisposed()) {
      db->EnterDisposedStateSync();
    }
    RejectErrInvalidState(env, args, "database is not open");
    return;
  }

  AsyncDispose(args);
}

void Database::AsyncDispose(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());

  // Disposing is idempotent, so if a close is already in progress or has been
  // completed, return the existing promise.
  Local<Value> maybe_dispose_promise =
      db->object()->GetInternalField(Database::kDisposePromiseSlot).As<Value>();
  if (maybe_dispose_promise->IsPromise()) {
    args.GetReturnValue().Set(maybe_dispose_promise.As<Promise>());
    return;
  }
  if (!db->IsOpen()) {
    args.GetReturnValue().Set(db->EnterDisposedStateSync());
    return;
  }

  // We don't need to dispose statements during destruction, because all
  // Statement instances keep a BaseObjectPtr to the Database and therefore
  // can't outlive the Database. Therefore this shouldn't be executed as part of
  // db->AsyncDisposeImpl().
  // Calling stmt->Dispose modifies the statements_ set, therefore make a copy.
  for (Statement* stmt : std::vector<Statement*>(db->statements_.begin(),
                                                 db->statements_.end())) {
    stmt->Dispose();
  }

  args.GetReturnValue().Set(db->AsyncDisposeImpl());
}

void Database::Prepare(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env, args, !db->IsOpen(), "database is not open");

  REJECT_AND_RETURN_ON_INVALID_ARG_TYPE(
      env,
      args,
      !args[0]->IsString(),
      "The \"sql\" argument must be a string.");
  Utf8Value sql(env->isolate(), args[0].As<String>());

  statement_options options{
      .return_arrays = db->open_config_.get_return_arrays(),
      .read_big_ints = db->open_config_.get_use_big_ints(),
  };
  if (args.Length() > 1) {
    REJECT_AND_RETURN_ON_INVALID_ARG_TYPE(
        env,
        args,
        !args[1]->IsObject(),
        "The \"options\" argument must be an object.");
    Local<Object> options_object = args[1].As<Object>();
    Local<Value> value;

    if (options_object
            ->Get(env->context(),
                  FIXED_ONE_BYTE_STRING(env->isolate(), "returnArrays"))
            .ToLocal(&value) &&
        !value->IsUndefined()) {
      REJECT_AND_RETURN_ON_INVALID_ARG_TYPE(
          env,
          args,
          !value->IsBoolean(),
          "The \"returnArrays\" option must be a boolean.");
      options.return_arrays = value.As<Boolean>()->Value();
    }

    if (options_object
            ->Get(env->context(),
                  FIXED_ONE_BYTE_STRING(env->isolate(), "readBigInts"))
            .ToLocal(&value) &&
        !value->IsUndefined()) {
      REJECT_AND_RETURN_ON_INVALID_ARG_TYPE(
          env,
          args,
          !value->IsBoolean(),
          "The \"readBigInts\" option must be a boolean.");
      options.read_big_ints = value.As<Boolean>()->Value();
    }
  }

  args.GetReturnValue().Set(db->Schedule<PrepareStatementOperation>(
      BaseObjectPtr<Database>(db), std::string(*sql, sql.length()), options));
}

void Database::TrackStatement(Statement* statement) {
  statements_.insert(statement);
}
void Database::UntrackStatement(Statement* statement) {
  statements_.erase(statement);
}

void Database::Exec(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env, args, !db->IsOpen(), "database is not open");
  REJECT_AND_RETURN_ON_INVALID_ARG_TYPE(
      env,
      args,
      args.Length() < 1 || !args[0]->IsString(),
      "The \"sql\" argument must be a string.");

  Utf8Value sql(env->isolate(), args[0].As<String>());
  args.GetReturnValue().Set(
      db->Schedule<ExecOperation>(std::string(*sql, sql.length())));
}

void Database::IsInTransaction(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env, args, !db->IsOpen(), "database is not open");

  args.GetReturnValue().Set(db->Schedule<IsInTransactionOperation>());
}

Statement::Statement(Environment* env,
                     v8::Local<v8::Object> object,
                     BaseObjectPtr<Database> db,
                     sqlite3_stmt* stmt,
                     statement_options options)
    : BaseObject(env, object),
      db_(std::move(db)),
      statement_(stmt),
      options_(options) {
  MakeWeak();
  if (stmt == nullptr) {
    db_ = nullptr;
  } else {
    CHECK_NOT_NULL(db_);
    db_->TrackStatement(this);
  }
}

void Database::Location(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env, args, !db->IsOpen(), "database is not open");

  std::string db_name;
  if (args.Length() > 0) {
    REJECT_AND_RETURN_ON_INVALID_ARG_TYPE(
        env,
        args,
        !args[0]->IsString(),
        "The \"dbName\" argument must be a string.");
    Utf8Value db_name_utf8(env->isolate(), args[0].As<String>());
    db_name = std::string(*db_name_utf8, db_name_utf8.length());
  } else {
    db_name = "main";
  }
  args.GetReturnValue().Set(
      db->Schedule<LocationOperation>(std::move(db_name)));
}

void Database::EnableDefensive(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env, args, !db->IsOpen(), "database is not open");

  REJECT_AND_RETURN_ON_INVALID_ARG_TYPE(
      env,
      args,
      !args[0]->IsBoolean() || args.Length() != 1,
      "\"enableDefensive\" requires exactly one boolean argument.");

  auto enable_defensive = args[0].As<Boolean>()->Value();
  args.GetReturnValue().Set(db->Schedule<UpdateDbConfigOperation>(
      SQLITE_DBCONFIG_DEFENSIVE, enable_defensive ? 1 : 0));
}

void Database::EnableLoadExtension(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env, args, !db->IsOpen(), "database is not open");

  REJECT_AND_RETURN_ON_INVALID_ARG_TYPE(
      env,
      args,
      !args[0]->IsBoolean() || args.Length() != 1,
      "\"enableLoadExtension\" requires exactly one boolean argument.");

  auto enable_load_extension = args[0].As<Boolean>()->Value();
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env,
      args,
      !db->allow_load_extension_ && enable_load_extension,
      "Cannot enable extension loading because it was disabled at database "
      "creation.");

  db->enable_load_extension_ = enable_load_extension;
  args.GetReturnValue().Set(db->Schedule<UpdateDbConfigOperation>(
      SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, enable_load_extension ? 1 : 0));
}

void Database::LoadExtension(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Database* db;
  ASSIGN_OR_RETURN_UNWRAP(&db, args.This());
  Environment* env = Environment::GetCurrent(args);
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env, args, !db->IsOpen(), "database is not open");
  REJECT_AND_RETURN_ON_INVALID_STATE(env,
                                     args,
                                     !db->enable_load_extension_,
                                     "extension loading is not allowed");

  REJECT_AND_RETURN_ON_INVALID_ARG_TYPE(
      env,
      args,
      !args[0]->IsString() || args.Length() != 1,
      "\"loadExtension\" requires exactly one string argument.");

  BufferValue path(env->isolate(), args[0]);
  ToNamespacedPath(env, &path);

  args.GetReturnValue().Set(
      db->Schedule<LoadExtensionOperation>(std::string(path.ToStringView())));
}

Statement::~Statement() {
  if (!IsDisposed()) {
    // Our operations keep a BaseObjectPtr to this Statement, so we can be sure
    // that no operations are running or will run after this point. The only
    // exception to this is the FinalizeStatementOperation, but it can only be
    // queued by Statement::Dispose.
    sqlite3_finalize(statement_);
    db_->UntrackStatement(this);
  }
}

void Statement::MemoryInfo(MemoryTracker* tracker) const {}

Local<FunctionTemplate> Statement::GetConstructorTemplate(Environment* env) {
  Local<FunctionTemplate> tmpl = env->sqlite_statement_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "Statement"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Statement::kInternalFieldCount);

    SetProtoDispose(isolate, tmpl, Statement::Dispose);
    SetSideEffectFreeGetter(isolate,
                            tmpl,
                            FIXED_ONE_BYTE_STRING(isolate, "isDisposed"),
                            Statement::IsDisposedGetter);
    SetProtoMethod(isolate, tmpl, "get", Statement::Get);
    SetProtoMethod(isolate, tmpl, "all", Statement::All);
    SetProtoMethod(isolate, tmpl, "run", Statement::Run);

    env->set_sqlite_statement_constructor_template(tmpl);
  }
  return tmpl;
}
BaseObjectPtr<Statement> Statement::Create(Environment* env,
                                           BaseObjectPtr<Database> db,
                                           sqlite3_stmt* stmt,
                                           statement_options options) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<Statement>(env, obj, std::move(db), stmt, options);
}

void Statement::Dispose(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  stmt->Dispose();
  args.GetReturnValue().SetUndefined();
}

void Statement::Dispose() {
  if (IsDisposed()) {
    return;
  }
  // Finalizing is a no-fail operation, so we don't need to check the result or
  // return a promise.
  (void)db_->Schedule<FinalizeStatementOperation>(
      std::exchange(statement_, nullptr));
  std::exchange(db_, nullptr)->UntrackStatement(this);
}

void Statement::IsDisposedGetter(const FunctionCallbackInfo<Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  args.GetReturnValue().Set(Boolean::New(env->isolate(), stmt->IsDisposed()));
}

bool Statement::IsDisposed() const {
  return statement_ == nullptr;
}

void Statement::Get(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env, args, stmt->IsDisposed(), "statement is disposed");

  transfer::value bind_arguments;
  if (args.Length() >= 1) {
    TryCatch try_catch(env->isolate());
    if (!transfer::ToValue(env->isolate(), args[0].As<Value>())
             .MoveTo(&bind_arguments)) {
      if (try_catch.HasCaught() && try_catch.CanContinue()) {
        RejectErr(env, args, try_catch.Exception());
      }
      return;
    }
  }

  args.GetReturnValue().Set(stmt->db_->Schedule<StatementGetOperation>(
      stmt->statement_, std::move(bind_arguments), stmt->options_));
}

void Statement::All(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env, args, stmt->IsDisposed(), "statement is disposed");

  transfer::value bind_arguments;
  if (args.Length() >= 1) {
    TryCatch try_catch(env->isolate());
    if (!transfer::ToValue(env->isolate(), args[0].As<Value>())
             .MoveTo(&bind_arguments)) {
      if (try_catch.HasCaught() && try_catch.CanContinue()) {
        RejectErr(env, args, try_catch.Exception());
      }
      return;
    }
  }

  args.GetReturnValue().Set(stmt->db_->Schedule<StatementAllOperation>(
      stmt->statement_, std::move(bind_arguments), stmt->options_));
}

void Statement::Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Statement* stmt;
  ASSIGN_OR_RETURN_UNWRAP(&stmt, args.This());
  Environment* env = Environment::GetCurrent(args);
  REJECT_AND_RETURN_ON_INVALID_STATE(
      env, args, stmt->IsDisposed(), "statement is disposed");

  transfer::value bind_arguments;
  if (args.Length() >= 1) {
    TryCatch try_catch(env->isolate());
    if (!transfer::ToValue(env->isolate(), args[0].As<Value>())
             .MoveTo(&bind_arguments)) {
      if (try_catch.HasCaught() && try_catch.CanContinue()) {
        RejectErr(env, args, try_catch.Exception());
      }
      return;
    }
  }

  args.GetReturnValue().Set(
      stmt->db_->Schedule<StatementRunOperation>(stmt->statement_,
                                                 std::move(bind_arguments),
                                                 stmt->options_.read_big_ints));
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

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  Local<Object> constants = Object::New(isolate);

  DefineConstants(constants);

  SetConstructorFunction(context,
                         target,
                         "DatabaseSync",
                         CreateDatabaseSyncConstructorTemplate(env));
  SetConstructorFunction(context,
                         target,
                         "StatementSync",
                         StatementSync::GetConstructorTemplate(env));
  SetConstructorFunction(
      context, target, "Session", Session::GetConstructorTemplate(env));
  SetConstructorFunction(
      context, target, "Database", CreateDatabaseConstructorTemplate(env));
  SetConstructorFunction(
      context, target, "Statement", Statement::GetConstructorTemplate(env));

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
