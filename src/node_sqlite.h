#ifndef SRC_NODE_SQLITE_H_
#define SRC_NODE_SQLITE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "lru_cache-inl.h"
#include "node_mem.h"
#include "sqlite3.h"
#include "threadpoolwork-inl.h"
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

  inline bool get_async() const { return async_; }

  inline void set_async(bool flag) { async_ = flag; }

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

 private:
  std::string location_;
  bool async_ = true;
  bool read_only_ = false;
  bool enable_foreign_keys_ = true;
  bool enable_dqs_ = false;
  int timeout_ = 0;
  bool use_big_ints_ = false;
  bool return_arrays_ = false;
  bool allow_bare_named_params_ = true;
  bool allow_unknown_named_params_ = false;
};

class Database;
class StatementIterator;
class Statement;
class BackupJob;

using RowArray = std::vector<sqlite3_value*>;
using RowObject = std::vector<std::pair<std::string, sqlite3_value*>>;
using Row = std::variant<RowArray, RowObject>;

struct SQLiteResult {
  int code;
  std::vector<Row> rows;
};

class StatementSQLiteToJSConverter {
 public:
  static v8::MaybeLocal<v8::Object> ConvertStatementRun(
      Environment* env,
      bool use_big_ints,
      sqlite3_int64 changes,
      sqlite3_int64 last_insert_rowid);
  // static v8::Local<v8::Value> ConvertAll(Environment* env,
  //                                        const QueryResult& result,
  //                                        bool return_arrays);
  // static v8::Local<v8::Object> ConvertGet(Environment* env,
  //                                         sqlite3_stmt* stmt,
  //                                         bool use_big_ints);
};

class SQLiteStatementExecutor {
 public:
  static std::optional<SQLiteResult> ExecuteGet(sqlite3_stmt* stmt);
};

class StatementAsyncExecutionHelper {
 public:
  static v8::MaybeLocal<v8::Promise::Resolver> Run(Environment* env,
                                                   Statement* stmt);
};

class StatementExecutionHelper {
 public:
  static v8::Local<v8::Value> All(Environment* env,
                                  Database* db,
                                  sqlite3_stmt* stmt,
                                  bool return_arrays,
                                  bool use_big_ints);
  static v8::Local<v8::Object> Run(Environment* env,
                                   Database* db,
                                   sqlite3_stmt* stmt,
                                   bool use_big_ints);
  static BaseObjectPtr<StatementIterator> Iterate(
      Environment* env, BaseObjectPtr<Statement> stmt);
  static v8::MaybeLocal<v8::Value> ColumnToValue(Environment* env,
                                                 sqlite3_stmt* stmt,
                                                 const int column,
                                                 bool use_big_ints);
  static v8::MaybeLocal<v8::Name> ColumnNameToName(Environment* env,
                                                   sqlite3_stmt* stmt,
                                                   const int column);
  static v8::Local<v8::Value> Get(Environment* env,
                                  Database* db,
                                  sqlite3_stmt* stmt,
                                  bool return_arrays,
                                  bool use_big_ints);
};

class Database : public BaseObject {
 public:
  Database(Environment* env,
           v8::Local<v8::Object> object,
           DatabaseOpenConfiguration&& open_config,
           bool open,
           bool allow_load_extension);
  void MemoryInfo(MemoryTracker* tracker) const override;
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void NewAsync(const v8::FunctionCallbackInfo<v8::Value>& args);
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
  static void LoadExtension(const v8::FunctionCallbackInfo<v8::Value>& args);
  void FinalizeStatements();
  void RemoveBackup(BackupJob* backup);
  void AddBackup(BackupJob* backup);
  void AddAsyncTask(ThreadPoolWork* async_task);
  void RemoveAsyncTask(ThreadPoolWork* async_task);
  void FinalizeBackups();
  void UntrackStatement(Statement* statement);
  bool IsOpen();
  bool is_async() { return open_config_.get_async(); }
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

  SET_MEMORY_INFO_NAME(Database)
  SET_SELF_SIZE(Database)

 private:
  bool Open();
  void DeleteSessions();

  ~Database() override;
  DatabaseOpenConfiguration open_config_;
  bool allow_load_extension_;
  bool enable_load_extension_;
  sqlite3* connection_;
  bool ignore_next_sqlite_error_;

  std::set<ThreadPoolWork*> async_tasks_;
  std::set<BackupJob*> backups_;
  std::set<sqlite3_session*> sessions_;
  std::unordered_set<Statement*> statements_;

  friend class Session;
  friend class SQLTagStore;
  friend class StatementExecutionHelper;
  friend class StatementAsyncExecutionHelper;
};

class Statement : public BaseObject {
 public:
  Statement(Environment* env,
            v8::Local<v8::Object> object,
            BaseObjectPtr<Database> db,
            sqlite3_stmt* stmt);
  void MemoryInfo(MemoryTracker* tracker) const override;
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env, std::string_view class_name);
  static BaseObjectPtr<Statement> Create(Environment* env,
                                         BaseObjectPtr<Database> db,
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

  SET_MEMORY_INFO_NAME(Statement)
  SET_SELF_SIZE(Statement)

 private:
  ~Statement() override;
  BaseObjectPtr<Database> db_;
  sqlite3_stmt* statement_;
  bool return_arrays_ = false;
  bool use_big_ints_;
  bool allow_bare_named_params_;
  bool allow_unknown_named_params_;
  std::optional<std::map<std::string, std::string>> bare_named_params_;
  bool BindParams(const v8::FunctionCallbackInfo<v8::Value>& args);
  bool BindValue(const v8::Local<v8::Value>& value, const int index);

  friend class StatementIterator;
  friend class SQLTagStore;
  friend class StatementExecutionHelper;
  friend class StatementAsyncExecutionHelper;
};

class StatementIterator : public BaseObject {
 public:
  StatementIterator(Environment* env,
                    v8::Local<v8::Object> object,
                    BaseObjectPtr<Statement> stmt);
  void MemoryInfo(MemoryTracker* tracker) const override;
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static BaseObjectPtr<StatementIterator> Create(Environment* env,
                                                 BaseObjectPtr<Statement> stmt);
  static void Next(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Return(const v8::FunctionCallbackInfo<v8::Value>& args);

  SET_MEMORY_INFO_NAME(StatementIterator)
  SET_SELF_SIZE(StatementIterator)

 private:
  ~StatementIterator() override;
  BaseObjectPtr<Statement> stmt_;
  bool done_;
};

using Sqlite3ChangesetGenFunc = int (*)(sqlite3_session*, int*, void**);

class Session : public BaseObject {
 public:
  Session(Environment* env,
          v8::Local<v8::Object> object,
          BaseObjectWeakPtr<Database> database,
          sqlite3_session* session);
  ~Session() override;
  template <Sqlite3ChangesetGenFunc sqliteChangesetFunc>
  static void Changeset(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Dispose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static BaseObjectPtr<Session> Create(Environment* env,
                                       BaseObjectWeakPtr<Database> database,
                                       sqlite3_session* session);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Session)
  SET_SELF_SIZE(Session)

 private:
  void Delete();
  sqlite3_session* session_;
  BaseObjectWeakPtr<Database> database_;  // The Parent Database
};

class SQLTagStore : public BaseObject {
 public:
  SQLTagStore(Environment* env,
              v8::Local<v8::Object> object,
              BaseObjectWeakPtr<Database> database,
              int capacity);
  ~SQLTagStore() override;
  static BaseObjectPtr<SQLTagStore> Create(Environment* env,
                                           BaseObjectWeakPtr<Database> database,
                                           int capacity);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void All(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void Get(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void Iterate(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void Run(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void Size(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void Capacity(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void Reset(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void Clear(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void DatabaseGetter(const v8::FunctionCallbackInfo<v8::Value>& info);
  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(SQLTagStore)
  SET_SELF_SIZE(SQLTagStore)

 private:
  static BaseObjectPtr<Statement> PrepareStatement(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  BaseObjectWeakPtr<Database> database_;
  LRUCache<std::string, BaseObjectPtr<Statement>> sql_tags_;
  int capacity_;
  friend class StatementExecutionHelper;
};

class UserDefinedFunction {
 public:
  UserDefinedFunction(Environment* env,
                      v8::Local<v8::Function> fn,
                      Database* db,
                      bool use_bigint_args);
  ~UserDefinedFunction();
  static void xFunc(sqlite3_context* ctx, int argc, sqlite3_value** argv);
  static void xDestroy(void* self);

 private:
  Environment* env_;
  v8::Global<v8::Function> fn_;
  Database* db_;
  bool use_bigint_args_;
};

}  // namespace sqlite
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_SQLITE_H_
