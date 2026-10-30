#ifndef SRC_NODE_WEBSTORAGE_H_
#define SRC_NODE_WEBSTORAGE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <optional>
#include <unordered_map>
#include "base_object.h"
#include "node_mem.h"
#include "sqlite3.h"
#include "util.h"

namespace node {
namespace webstorage {

struct conn_deleter {
  void operator()(sqlite3* conn) const noexcept {
    CHECK_EQ(sqlite3_close(conn), SQLITE_OK);
  }
};
using conn_unique_ptr = std::unique_ptr<sqlite3, conn_deleter>;

struct stmt_deleter {
  void operator()(sqlite3_stmt* stmt) const noexcept { sqlite3_finalize(stmt); }
};
using stmt_unique_ptr = std::unique_ptr<sqlite3_stmt, stmt_deleter>;

static constexpr std::string_view kInMemoryPath = ":memory:";

class Storage;

class BindingData : public BaseObject {
 public:
  SET_BINDING_ID(webstorage_binding_data)

  BindingData(Realm* realm, v8::Local<v8::Object> wrap);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(BindingData)
  SET_SELF_SIZE(BindingData)

  BaseObjectWeakPtr<Storage> local_storage;
  BaseObjectWeakPtr<Storage> session_storage;
};

class Storage : public BaseObject {
 public:
  Storage(Environment* env,
          v8::Local<v8::Object> object,
          std::string_view location);
  void MemoryInfo(MemoryTracker* tracker) const override;
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  v8::Maybe<void> Clear();
  v8::MaybeLocal<v8::Array> Enumerate();
  v8::MaybeLocal<v8::Value> Length();
  v8::MaybeLocal<v8::Value> Load(v8::Local<v8::Name> key);
  v8::MaybeLocal<v8::Value> LoadKey(const int index);
  v8::Maybe<void> Remove(v8::Local<v8::Name> key);
  v8::Maybe<void> Store(v8::Local<v8::Name> key, v8::Local<v8::Value> value);
  using StorageMap = std::unordered_map<std::u16string, std::u16string>;
  // Returns nothing if the backing store could not be read, e.g. because it
  // holds values of an unexpected type. If opening the store fails, *error is
  // populated with the failure reason instead of throwing a JS exception, so
  // that this can be called from V8 interrupts where JS execution is forbidden.
  std::optional<StorageMap> GetAll(std::string* error = nullptr);
  static std::optional<StorageMap> ReadAllFromFile(std::string_view location,
                                                   std::string* error);

  SET_MEMORY_INFO_NAME(Storage)
  SET_SELF_SIZE(Storage)

 private:
  v8::Maybe<void> Open();
  static conn_unique_ptr OpenConnection(std::string_view location,
                                        std::string* error);

  ~Storage() override;
  std::string location_;
  conn_unique_ptr db_;
  v8::Global<v8::Map> symbols_;
};

}  // namespace webstorage
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_WEBSTORAGE_H_
