#ifndef SRC_NODE_SQLITE_H_
#define SRC_NODE_SQLITE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "lru_cache-inl.h"
#include "node_mem.h"
#include "sqlite3.h"
#include "util.h"

#include <list>
#include <map>
#include <unordered_set>

namespace node {
namespace sqlite {

class DatabaseOpenConfiguration {
 public:
  explicit DatabaseOpenConfiguration(std::string&& location)
      : location_(std::move(location)) {}

  inline const std::string& location() const { return location_; }

  inline bool get_read_only() const { return read_only_; }

  inline void set_read_only(bool flag) { read_only_ = flag; }

  inline bool get_enable_foreign_keys() const { return enable_foreign_keys_; }

  inline void set_enable_foreign_keys(bool flag) {
    enable_foreign_keys_ = flag;
  }

  inline bool get_enable_dqs() const { return enable_dqs_; }

  inline void set_enable_dqs(bool flag) { enable_dqs_ = flag; }

  inline void set_timeout(int timeout) { timeout_ = timeout; }

  inline int get_timeout() { return timeout_; }

  inline void set_use_big_ints(bool flag) { use_big_ints_ = flag; }

  inline bool get_use_big_ints() const { return use_big_ints_; }

  inline void set_return_arrays(bool flag) { return_arrays_ = flag; }

  inline bool get_return_arrays() const { return return_arrays_; }

  inline void set_allow_bare_named_params(bool flag) {
    allow_bare_named_params_ = flag;
  }

  inline bool get_allow_bare_named_params() const {
    return allow_bare_named_params_;
  }

  inline void set_allow_unknown_named_params(bool flag) {
    allow_unknown_named_params_ = flag;
  }

  inline bool get_allow_unknown_named_params() const {
    return allow_unknown_named_params_;
  }

  inline void set_enable_defensive(bool flag) { defensive_ = flag; }

  inline bool get_enable_defensive() const { return defensive_; }

 private:
  std::string location_;
  bool read_only_ = false;
  bool enable_foreign_keys_ = true;
  bool enable_dqs_ = false;
  int timeout_ = 0;
  bool use_big_ints_ = false;
  bool return_arrays_ = false;
  bool allow_bare_named_params_ = true;
  bool allow_unknown_named_params_ = false;
  bool defensive_ = false;
};

class DatabaseSync;
class StatementSyncIterator;
class StatementSync;
class BackupJob;

class StatementExecutionHelper {
 public:
  static v8::MaybeLocal<v8::Value> All(Environment* env,
                                       DatabaseSync* db,
                                       sqlite3_stmt* stmt,
                                       bool return_arrays,
                                       bool use_big_ints);
  static v8::MaybeLocal<v8::Object> Run(Environment* env,
                                        DatabaseSync* db,
                                        sqlite3_stmt* stmt,
                                        bool use_big_ints);
  static BaseObjectPtr<StatementSyncIterator> Iterate(
      Environment* env, BaseObjectPtr<StatementSync> stmt);
  static v8::MaybeLocal<v8::Value> ColumnToValue(Environment* env,
                                                 sqlite3_stmt* stmt,
                                                 const int column,
                                                 bool use_big_ints);
  static v8::MaybeLocal<v8::Name> ColumnNameToName(Environment* env,
                                                   sqlite3_stmt* stmt,
                                                   const int column);
  static v8::MaybeLocal<v8::Value> Get(Environment* env,
                                       DatabaseSync* db,
                                       sqlite3_stmt* stmt,
                                       bool return_arrays,
                                       bool use_big_ints);
};

class DatabaseSync : public BaseObject {
 public:
  enum InternalFields {
    kAuthorizerCallback = BaseObject::kInternalFieldCount,
    kInternalFieldCount
  };

  DatabaseSync(Environment* env,
               v8::Local<v8::Object> object,
               DatabaseOpenConfiguration&& open_config,
               bool open,
               bool allow_load_extension);
  void MemoryInfo(MemoryTracker* tracker) const override;
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Open(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsOpenGetter(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsTransactionGetter(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Dispose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Prepare(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Exec(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CreateTagStore(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Location(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CustomFunction(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AggregateFunction(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CreateSession(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ApplyChangeset(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EnableLoadExtension(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EnableDefensive(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LoadExtension(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAuthorizer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static int AuthorizerCallback(void* user_data,
                                int action_code,
                                const char* param1,
                                const char* param2,
                                const char* param3,
                                const char* param4);
  void FinalizeStatements();
  void RemoveBackup(BackupJob* backup);
  void AddBackup(BackupJob* backup);
  void FinalizeBackups();
  void UntrackStatement(StatementSync* statement);
  bool IsOpen();
  bool use_big_ints() const { return open_config_.get_use_big_ints(); }
  bool return_arrays() const { return open_config_.get_return_arrays(); }
  bool allow_bare_named_params() const {
    return open_config_.get_allow_bare_named_params();
  }
  bool allow_unknown_named_params() const {
    return open_config_.get_allow_unknown_named_params();
  }
  sqlite3* Connection();

  // In some situations, such as when using custom functions, it is possible
  // that SQLite reports an error while JavaScript already has a pending
  // exception. In this case, the SQLite error should be ignored. These methods
  // enable that use case.
  void SetIgnoreNextSQLiteError(bool ignore);
  bool ShouldIgnoreSQLiteError();

  SET_MEMORY_INFO_NAME(DatabaseSync)
  SET_SELF_SIZE(DatabaseSync)

 private:
  bool Open();
  void DeleteSessions();

  ~DatabaseSync() override;
  DatabaseOpenConfiguration open_config_;
  bool allow_load_extension_;
  bool enable_load_extension_;
  sqlite3* connection_;
  bool ignore_next_sqlite_error_;

  std::set<BackupJob*> backups_;
  std::set<sqlite3_session*> sessions_;
  std::unordered_set<StatementSync*> statements_;

  friend class Session;
  friend class SQLTagStore;
  friend class StatementExecutionHelper;
};

class StatementSync : public BaseObject {
 public:
  StatementSync(Environment* env,
                v8::Local<v8::Object> object,
                BaseObjectPtr<DatabaseSync> db,
                sqlite3_stmt* stmt);
  void MemoryInfo(MemoryTracker* tracker) const override;
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static BaseObjectPtr<StatementSync> Create(Environment* env,
                                             BaseObjectPtr<DatabaseSync> db,
                                             sqlite3_stmt* stmt);
  static void All(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Iterate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Get(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Run(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Columns(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SourceSQLGetter(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ExpandedSQLGetter(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAllowBareNamedParameters(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAllowUnknownNamedParameters(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetReadBigInts(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetReturnArrays(const v8::FunctionCallbackInfo<v8::Value>& args);
  v8::MaybeLocal<v8::Value> ColumnToValue(const int column);
  v8::MaybeLocal<v8::Name> ColumnNameToName(const int column);
  void Finalize();
  bool IsFinalized();

  SET_MEMORY_INFO_NAME(StatementSync)
  SET_SELF_SIZE(StatementSync)

 private:
  ~StatementSync() override;
  BaseObjectPtr<DatabaseSync> db_;
  sqlite3_stmt* statement_;
  bool return_arrays_ = false;
  bool use_big_ints_;
  bool allow_bare_named_params_;
  bool allow_unknown_named_params_;
  std::optional<std::map<std::string, std::string>> bare_named_params_;
  bool BindParams(const v8::FunctionCallbackInfo<v8::Value>& args);
  bool BindValue(const v8::Local<v8::Value>& value, const int index);

  friend class StatementSyncIterator;
  friend class SQLTagStore;
  friend class StatementExecutionHelper;
};

class StatementSyncIterator : public BaseObject {
 public:
  StatementSyncIterator(Environment* env,
                        v8::Local<v8::Object> object,
                        BaseObjectPtr<StatementSync> stmt);
  void MemoryInfo(MemoryTracker* tracker) const override;
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static BaseObjectPtr<StatementSyncIterator> Create(
      Environment* env, BaseObjectPtr<StatementSync> stmt);
  static void Next(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Return(const v8::FunctionCallbackInfo<v8::Value>& args);

  SET_MEMORY_INFO_NAME(StatementSyncIterator)
  SET_SELF_SIZE(StatementSyncIterator)

 private:
  ~StatementSyncIterator() override;
  BaseObjectPtr<StatementSync> stmt_;
  bool done_;
};

using Sqlite3ChangesetGenFunc = int (*)(sqlite3_session*, int*, void**);

class Session : public BaseObject {
 public:
  Session(Environment* env,
          v8::Local<v8::Object> object,
          BaseObjectWeakPtr<DatabaseSync> database,
          sqlite3_session* session);
  ~Session() override;
  template <Sqlite3ChangesetGenFunc sqliteChangesetFunc>
  static void Changeset(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Dispose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static BaseObjectPtr<Session> Create(Environment* env,
                                       BaseObjectWeakPtr<DatabaseSync> database,
                                       sqlite3_session* session);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Session)
  SET_SELF_SIZE(Session)

 private:
  void Delete();
  sqlite3_session* session_;
  BaseObjectWeakPtr<DatabaseSync> database_;  // The Parent Database
};

class SQLTagStore : public BaseObject {
 public:
  SQLTagStore(Environment* env,
              v8::Local<v8::Object> object,
              BaseObjectWeakPtr<DatabaseSync> database,
              int capacity);
  ~SQLTagStore() override;
  static BaseObjectPtr<SQLTagStore> Create(
      Environment* env, BaseObjectWeakPtr<DatabaseSync> database, int capacity);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void All(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Get(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Iterate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Run(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Clear(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CapacityGetter(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DatabaseGetter(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SizeGetter(const v8::FunctionCallbackInfo<v8::Value>& args);
  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(SQLTagStore)
  SET_SELF_SIZE(SQLTagStore)

 private:
  static BaseObjectPtr<StatementSync> PrepareStatement(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  BaseObjectWeakPtr<DatabaseSync> database_;
  LRUCache<std::string, BaseObjectPtr<StatementSync>> sql_tags_;
  int capacity_;
  friend class StatementExecutionHelper;
};

class UserDefinedFunction {
 public:
  UserDefinedFunction(Environment* env,
                      v8::Local<v8::Function> fn,
                      DatabaseSync* db,
                      bool use_bigint_args);
  ~UserDefinedFunction();
  static void xFunc(sqlite3_context* ctx, int argc, sqlite3_value** argv);
  static void xDestroy(void* self);

 private:
  Environment* env_;
  v8::Global<v8::Function> fn_;
  DatabaseSync* db_;
  bool use_bigint_args_;
};

}  // namespace sqlite
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_SQLITE_H_
