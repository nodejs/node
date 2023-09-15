#ifndef SRC_NODE_WEBSTORAGE_H_
#define SRC_NODE_WEBSTORAGE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "node_mem.h"
#include "sqlite3.h"

namespace node {
namespace webstorage {

class Storage : public BaseObject {
 public:
  Storage(
      Environment* env,
      v8::Local<v8::Object> object,
      v8::Local<v8::String> location);
  void MemoryInfo(MemoryTracker* tracker) const override;
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  void Clear();
  v8::Local<v8::Array> Enumerate();
  v8::Local<v8::Value> Length();
  v8::Local<v8::Value> Load(v8::Local<v8::Name> key);
  v8::Local<v8::Value> LoadKey(const int index);
  bool Remove(v8::Local<v8::Name> key);
  bool Store(v8::Local<v8::Name> key, v8::Local<v8::Value> value);

  SET_MEMORY_INFO_NAME(Storage)
  SET_SELF_SIZE(Storage)

 private:
  bool Open();

  ~Storage() override;
  std::string _location;
  sqlite3* _db;
  v8::Global<v8::Map> _symbols;
};


}  // namespace webstorage
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_WEBSTORAGE_H_
