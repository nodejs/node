// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An in-memory IndexedDB.
//
// What is modelled: databases with object stores and indexes, key paths, key
// generators, key ranges, cursors, and transactions whose requests complete
// in separate tasks and which commit once they have nothing left to do.
// Values are stored structured-cloned (v8::ValueSerializer) and read back
// with v8::ValueDeserializer, so that the V8 side of a put or get costs what
// it costs in a browser.
//
// What is not: persistence (everything lives until the process exits),
// blocking between connections (a version change or deletion never waits for
// other connections to close), and rolling back schema changes when an
// upgrade transaction aborts.

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "experimental/d8/mock-dom/bindings.h"
#include "experimental/d8/mock-dom/isolate-data.h"
#include "experimental/d8/mock-dom/objects.h"
#include "include/v8-array-buffer.h"
#include "include/v8-container.h"
#include "include/v8-date.h"
#include "include/v8-exception.h"
#include "include/v8-primitive.h"
#include "include/v8-promise.h"
#include "include/v8-value-serializer.h"

namespace v8 {
namespace mock_dom {

namespace {

#define CALLBACK(name) void name(const CallbackInfo& info)

// --- Keys --------------------------------------------------------------------

struct IDBKey {
  // In the order keys of different types sort.
  // kMax is not a key: like kInvalid at the other end, it only serves as a
  // search bound.
  enum Type : uint8_t {
    kInvalid,
    kNumber,
    kDate,
    kString,
    kBinary,
    kArray,
    kMax
  };
  Type type = kInvalid;
  double number = 0;
  std::u16string string;
  std::vector<uint8_t> binary;
  std::vector<IDBKey> array;

  bool valid() const { return type != kInvalid && type != kMax; }
};

int CompareKeys(const IDBKey& a, const IDBKey& b) {
  if (a.type != b.type) return a.type < b.type ? -1 : 1;
  switch (a.type) {
    case IDBKey::kInvalid:
    case IDBKey::kMax:
      return 0;
    case IDBKey::kNumber:
    case IDBKey::kDate:
      return a.number < b.number ? -1 : a.number > b.number ? 1 : 0;
    case IDBKey::kString:
      return a.string < b.string ? -1 : a.string > b.string ? 1 : 0;
    case IDBKey::kBinary:
      return a.binary < b.binary ? -1 : a.binary > b.binary ? 1 : 0;
    case IDBKey::kArray: {
      size_t n = std::min(a.array.size(), b.array.size());
      for (size_t i = 0; i < n; ++i) {
        int c = CompareKeys(a.array[i], b.array[i]);
        if (c != 0) return c;
      }
      return a.array.size() < b.array.size()   ? -1
             : a.array.size() > b.array.size() ? 1
                                               : 0;
    }
  }
  return 0;
}

struct KeyLess {
  bool operator()(const IDBKey& a, const IDBKey& b) const {
    return CompareKeys(a, b) < 0;
  }
};

// "Convert a value to a key". Returns an invalid key if |value| is not one.
IDBKey KeyFromValue(Isolate* isolate, Local<Value> value, int depth = 0) {
  IDBKey key;
  Local<Context> context = isolate->GetCurrentContext();
  if (value->IsNumber()) {
    double number = value.As<Number>()->Value();
    if (std::isnan(number)) return key;
    key.type = IDBKey::kNumber;
    key.number = number;
  } else if (value->IsDate()) {
    double time = value.As<Date>()->ValueOf();
    if (std::isnan(time)) return key;
    key.type = IDBKey::kDate;
    key.number = time;
  } else if (value->IsString()) {
    Local<v8::String> string = value.As<v8::String>();
    key.type = IDBKey::kString;
    key.string.resize(string->Length());
    string->Write(isolate, 0, string->Length(),
                  reinterpret_cast<uint16_t*>(key.string.data()));
  } else if (value->IsArrayBuffer() || value->IsArrayBufferView()) {
    const uint8_t* data;
    size_t size;
    if (value->IsArrayBuffer()) {
      Local<ArrayBuffer> buffer = value.As<ArrayBuffer>();
      data = static_cast<const uint8_t*>(buffer->Data());
      size = buffer->ByteLength();
    } else {
      Local<ArrayBufferView> view = value.As<ArrayBufferView>();
      data = static_cast<const uint8_t*>(view->GetBuffer()->Data()) +
             view->ByteOffset();
      size = view->ByteLength();
    }
    key.type = IDBKey::kBinary;
    key.binary.assign(data, data + size);
  } else if (value->IsArray() && depth < 32) {
    Local<Array> array = value.As<Array>();
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; ++i) {
      bool has = false;
      if (!array->HasOwnProperty(context, i).To(&has) || !has) return IDBKey();
      Local<Value> entry;
      if (!array->Get(context, i).ToLocal(&entry)) return IDBKey();
      IDBKey element = KeyFromValue(isolate, entry, depth + 1);
      if (!element.valid()) return IDBKey();
      key.array.push_back(std::move(element));
    }
    key.type = IDBKey::kArray;
  }
  return key;
}

Local<Value> KeyToValue(Isolate* isolate, const IDBKey& key) {
  Local<Context> context = isolate->GetCurrentContext();
  switch (key.type) {
    case IDBKey::kInvalid:
    case IDBKey::kMax:
      return Undefined(isolate);
    case IDBKey::kNumber:
      return Number::New(isolate, key.number);
    case IDBKey::kDate:
      return Date::New(context, key.number).ToLocalChecked();
    case IDBKey::kString:
      return v8::String::NewFromTwoByte(
                 isolate, reinterpret_cast<const uint16_t*>(key.string.data()),
                 NewStringType::kNormal, static_cast<int>(key.string.size()))
          .ToLocalChecked();
    case IDBKey::kBinary: {
      Local<ArrayBuffer> buffer = ArrayBuffer::New(isolate, key.binary.size());
      if (!key.binary.empty()) {
        memcpy(buffer->Data(), key.binary.data(), key.binary.size());
      }
      return buffer;
    }
    case IDBKey::kArray: {
      Local<Array> array =
          Array::New(isolate, static_cast<int>(key.array.size()));
      for (size_t i = 0; i < key.array.size(); ++i) {
        USE(array->CreateDataProperty(context, static_cast<uint32_t>(i),
                                      KeyToValue(isolate, key.array[i])));
      }
      return array;
    }
  }
  return Undefined(isolate);
}

struct KeyRange {
  IDBKey lower;  // Invalid: unbounded.
  IDBKey upper;
  bool lower_open = false;
  bool upper_open = false;

  bool Includes(const IDBKey& key) const {
    if (lower.valid()) {
      int c = CompareKeys(key, lower);
      if (c < 0 || (c == 0 && lower_open)) return false;
    }
    if (upper.valid()) {
      int c = CompareKeys(key, upper);
      if (c > 0 || (c == 0 && upper_open)) return false;
    }
    return true;
  }
};

// --- Stored data -------------------------------------------------------------
//
// Plain C++ with no references into the JS heap: keys and serialized values.

// A key path: null, one string, or an array of strings.
struct KeyPath {
  bool is_null = true;
  bool is_array = false;
  std::vector<std::string> paths;
};

struct IndexData {
  std::string name;
  KeyPath key_path;
  bool unique = false;
  bool multi_entry = false;
  // (index key, primary key), sorted.
  struct EntryLess {
    bool operator()(const std::pair<IDBKey, IDBKey>& a,
                    const std::pair<IDBKey, IDBKey>& b) const {
      int c = CompareKeys(a.first, b.first);
      return c != 0 ? c < 0 : CompareKeys(a.second, b.second) < 0;
    }
  };
  std::set<std::pair<IDBKey, IDBKey>, EntryLess> entries;
};

struct Record {
  std::vector<uint8_t> value;
  // The keys this record has in each index, in the order of
  // StoreData::indexes at the time it was written.
  std::vector<std::vector<IDBKey>> index_keys;
};

struct StoreData {
  std::string name;
  KeyPath key_path;
  bool auto_increment = false;
  double key_generator = 1;
  std::map<IDBKey, Record, KeyLess> records;
  std::vector<std::unique_ptr<IndexData>> indexes;

  IndexData* FindIndex(const std::string& index_name) const {
    for (const auto& index : indexes) {
      if (index->name == index_name) return index.get();
    }
    return nullptr;
  }
  int IndexPosition(const IndexData* index) const {
    for (size_t i = 0; i < indexes.size(); ++i) {
      if (indexes[i].get() == index) return static_cast<int>(i);
    }
    return -1;
  }

  void RemoveRecord(const IDBKey& key) {
    auto it = records.find(key);
    if (it == records.end()) return;
    for (size_t i = 0; i < indexes.size() && i < it->second.index_keys.size();
         ++i) {
      for (const IDBKey& index_key : it->second.index_keys[i]) {
        indexes[i]->entries.erase({index_key, key});
      }
    }
    records.erase(it);
  }
  void InsertRecord(const IDBKey& key, Record record) {
    RemoveRecord(key);
    for (size_t i = 0; i < indexes.size() && i < record.index_keys.size();
         ++i) {
      for (const IDBKey& index_key : record.index_keys[i]) {
        indexes[i]->entries.insert({index_key, key});
      }
    }
    records[key] = std::move(record);
  }
};

struct DatabaseData {
  std::string name;
  double version = 0;
  std::vector<std::shared_ptr<StoreData>> stores;

  std::shared_ptr<StoreData> FindStore(const std::string& store_name) const {
    for (const auto& store : stores) {
      if (store->name == store_name) return store;
    }
    return nullptr;
  }
};

std::map<std::string, std::shared_ptr<DatabaseData>>& Databases() {
  static auto* databases =
      new std::map<std::string, std::shared_ptr<DatabaseData>>();
  return *databases;
}

// --- Structured clone --------------------------------------------------------

class CloneDelegate final : public ValueSerializer::Delegate {
 public:
  explicit CloneDelegate(Isolate* isolate) : isolate_(isolate) {}
  void ThrowDataCloneError(Local<v8::String> message) override {
    v8::String::Utf8Value text(isolate_, message);
    ThrowDOMException(isolate_, "DataCloneError", *text ? *text : "");
  }

 private:
  Isolate* isolate_;
};

// Returns false if serialization threw.
bool Serialize(Isolate* isolate, Local<Value> value,
               std::vector<uint8_t>* out) {
  CloneDelegate delegate(isolate);
  ValueSerializer serializer(isolate, &delegate);
  serializer.WriteHeader();
  bool ok = false;
  if (!serializer.WriteValue(isolate->GetCurrentContext(), value).To(&ok) ||
      !ok) {
    return false;
  }
  std::pair<uint8_t*, size_t> buffer = serializer.Release();
  out->assign(buffer.first, buffer.first + buffer.second);
  free(buffer.first);
  return true;
}

Local<Value> Deserialize(Isolate* isolate, const std::vector<uint8_t>& data) {
  Local<Context> context = isolate->GetCurrentContext();
  ValueDeserializer deserializer(isolate, data.data(), data.size());
  Local<Value> value;
  bool ok = false;
  if (!deserializer.ReadHeader(context).To(&ok) || !ok ||
      !deserializer.ReadValue(context).ToLocal(&value)) {
    return Undefined(isolate);
  }
  return value;
}

// --- Key paths ---------------------------------------------------------------

// "Evaluate a key path on a value" for a single path such as "a.b". Returns
// false if the path does not resolve.
bool EvaluatePath(Isolate* isolate, Local<Value> value, const std::string& path,
                  Local<Value>* out) {
  Local<Context> context = isolate->GetCurrentContext();
  size_t start = 0;
  while (start < path.size()) {
    size_t dot = path.find('.', start);
    if (dot == std::string::npos) dot = path.size();
    std::string identifier = path.substr(start, dot - start);
    start = dot + 1;
    if (value->IsString() && identifier == "length") {
      value = Integer::New(isolate, value.As<v8::String>()->Length());
      continue;
    }
    if (!value->IsObject()) return false;
    Local<Object> object = value.As<Object>();
    if (object->IsArray() && identifier == "length") {
      value = Integer::NewFromUnsigned(isolate, object.As<Array>()->Length());
      continue;
    }
    Local<v8::String> name = ToV8StringFromUtf8(isolate, identifier);
    bool has = false;
    if (!object->HasOwnProperty(context, name).To(&has) || !has) return false;
    if (!object->Get(context, name).ToLocal(&value)) return false;
  }
  *out = value;
  return true;
}

// The key a key path yields for |value|; invalid if there is none.
IDBKey ExtractKey(Isolate* isolate, Local<Value> value,
                  const KeyPath& key_path) {
  if (!key_path.is_array) {
    Local<Value> result;
    if (!EvaluatePath(isolate, value, key_path.paths[0], &result)) {
      return IDBKey();
    }
    return KeyFromValue(isolate, result);
  }
  IDBKey key;
  for (const std::string& path : key_path.paths) {
    Local<Value> result;
    if (!EvaluatePath(isolate, value, path, &result)) return IDBKey();
    IDBKey element = KeyFromValue(isolate, result);
    if (!element.valid()) return IDBKey();
    key.array.push_back(std::move(element));
  }
  key.type = IDBKey::kArray;
  return key;
}

// Sets the property a (single, dotted) key path names on |value| to |key|,
// creating intermediate objects. Used for stores with a key generator.
void InjectKey(Isolate* isolate, Local<Value> value, const std::string& path,
               const IDBKey& key) {
  Local<Context> context = isolate->GetCurrentContext();
  if (!value->IsObject()) return;
  Local<Object> object = value.As<Object>();
  size_t start = 0;
  while (true) {
    size_t dot = path.find('.', start);
    bool last = dot == std::string::npos;
    Local<v8::String> name = ToV8StringFromUtf8(
        isolate, path.substr(start, last ? std::string::npos : dot - start));
    if (last) {
      USE(object->CreateDataProperty(context, name, KeyToValue(isolate, key)));
      return;
    }
    bool has = false;
    if (!object->HasOwnProperty(context, name).To(&has)) return;
    Local<Value> next;
    if (has && !object->Get(context, name).ToLocal(&next)) return;
    if (!has || !next->IsObject()) {
      next = Object::New(isolate);
      USE(object->CreateDataProperty(context, name, next));
    }
    object = next.As<Object>();
    start = dot + 1;
  }
}

bool ParseKeyPath(const CallbackInfo& info, Local<Value> value, KeyPath* out) {
  Isolate* isolate = info.GetIsolate();
  if (value->IsNullOrUndefined()) return true;
  out->is_null = false;
  if (value->IsArray()) {
    out->is_array = true;
    Local<Array> array = value.As<Array>();
    for (uint32_t i = 0; i < array->Length(); ++i) {
      Local<Value> entry;
      std::string path;
      if (!array->Get(isolate->GetCurrentContext(), i).ToLocal(&entry) ||
          !ToStdString(info, entry, &path)) {
        return false;
      }
      out->paths.push_back(std::move(path));
    }
    return true;
  }
  std::string path;
  if (!ToStdString(info, value, &path)) return false;
  out->paths.push_back(std::move(path));
  return true;
}

Local<Value> KeyPathToValue(Isolate* isolate, const KeyPath& key_path) {
  if (key_path.is_null) return Null(isolate);
  if (!key_path.is_array) {
    return ToV8StringFromUtf8(isolate, key_path.paths[0]);
  }
  Local<Array> array =
      Array::New(isolate, static_cast<int>(key_path.paths.size()));
  for (size_t i = 0; i < key_path.paths.size(); ++i) {
    USE(array->Set(isolate->GetCurrentContext(), static_cast<uint32_t>(i),
                   ToV8StringFromUtf8(isolate, key_path.paths[i])));
  }
  return array;
}

// --- Backend latency ---------------------------------------------------------
//
// In a browser the database lives in another process, and results come back
// later than tasks the page already has queued: Chrome 157 on a workstation
// takes about 0.7 ms for open() and deleteDatabase() and about 0.1 ms per
// request or commit, one after the other. Pages depend on that without
// knowing (a listener registered by a later script is in place before the
// database opens), and it decides whether completions land inside or after
// a measured step. So results are delivered through a queue that models one
// backend working through the operations at those speeds.

constexpr double kFactoryLatencyMs = 0.7;
constexpr double kRequestLatencyMs = 0.1;

void PostBackendTask(Isolate* isolate, double cost_ms,
                     std::function<void()> task) {
  static double backend_free_at_ms = 0;
  const double now = MonotonicNowMs();
  const double due = std::max(now, backend_free_at_ms) + cost_ms;
  backend_free_at_ms = due;
  PostDelayedTask(isolate, std::move(task), (due - now) / 1000);
}

// --- Script-visible objects --------------------------------------------------

class DOMStringList final : public ScriptWrappable {
 public:
  explicit DOMStringList(std::vector<std::string> items)
      : ScriptWrappable(Interface::kDOMStringList), items(std::move(items)) {
    std::sort(this->items.begin(), this->items.end());
  }
  std::vector<std::string> items;
};

class IDBKeyRangeObject final : public ScriptWrappable {
 public:
  explicit IDBKeyRangeObject(KeyRange range)
      : ScriptWrappable(Interface::kIDBKeyRange), range(std::move(range)) {}
  KeyRange range;
};

class IDBTransaction;

class IDBRequest final : public EventTarget {
 public:
  IDBRequest(Interface interface, Window* window)
      : EventTarget(interface), window(window) {}
  void Trace(Visitor* visitor) const override {
    EventTarget::Trace(visitor);
    visitor->Trace(window);
    visitor->Trace(source);
    visitor->Trace(transaction);
    visitor->Trace(result);
    visitor->Trace(error);
  }
  Window* WrapperWindow() const override { return window.Get(); }

  Member<Window> window;
  Member<ScriptWrappable> source;
  Member<IDBTransaction> transaction;
  TracedReference<Value> result;
  TracedReference<Value> error;
  bool done = false;
};

class IDBDatabase final : public EventTarget {
 public:
  IDBDatabase(Window* window, std::shared_ptr<DatabaseData> data)
      : EventTarget(Interface::kIDBDatabase),
        window(window),
        data(std::move(data)) {}
  void Trace(Visitor* visitor) const override;
  Window* WrapperWindow() const override { return window.Get(); }

  Member<Window> window;
  std::shared_ptr<DatabaseData> data;
  // The running upgrade transaction, if any.
  Member<IDBTransaction> upgrade_transaction;
  bool closed = false;
};

// Fires |event| at |target| and, if it bubbles, at |parents| in order.
void FireEvent(Isolate* isolate, EventTarget* target, Event* event,
               std::initializer_list<EventTarget*> parents = {}) {
  event->is_trusted = true;
  event->dispatching = true;
  event->target = target;
  event->current_target = target;
  event->phase = Event::kAtTarget;
  target->FireListeners(isolate, event, true, true);
  if (!event->propagation_stopped) {
    target->FireListeners(isolate, event, false, true);
  }
  if (event->bubbles) {
    event->phase = Event::kBubbling;
    for (EventTarget* parent : parents) {
      if (event->propagation_stopped || !parent) break;
      event->current_target = parent;
      parent->FireListeners(isolate, event, false, false);
    }
  }
  event->phase = Event::kNone;
  event->current_target = nullptr;
  event->dispatching = false;
  event->propagation_stopped = false;
  event->immediate_propagation_stopped = false;
}

Event* NewEvent(Isolate* isolate, const char* type, bool bubbles = false,
                bool cancelable = false) {
  Event* event = MakeGarbageCollected<Event>(
      isolate, isolate, Interface::kEvent, AtomicString(type));
  event->bubbles = bubbles;
  event->cancelable = cancelable;
  return event;
}

Event* NewVersionChangeEvent(Isolate* isolate, const char* type,
                             double old_version, Local<Value> new_version) {
  Event* event = MakeGarbageCollected<Event>(
      isolate, isolate, Interface::kIDBVersionChangeEvent, AtomicString(type));
  event->set_number("oldVersion", old_version);
  event->any_slots[event->FieldIndex("newVersion")].Reset(isolate, new_version);
  return event;
}

class IDBTransaction final : public EventTarget {
 public:
  enum Mode : uint8_t { kReadOnly, kReadWrite, kVersionChange };

  IDBTransaction(Window* window, IDBDatabase* db, Mode mode,
                 std::vector<std::string> scope)
      : EventTarget(Interface::kIDBTransaction),
        window(window),
        db(db),
        mode(mode),
        scope(std::move(scope)) {}
  void Trace(Visitor* visitor) const override {
    EventTarget::Trace(visitor);
    visitor->Trace(window);
    visitor->Trace(db);
    visitor->Trace(open_request);
    visitor->Trace(error);
  }
  Window* WrapperWindow() const override { return window.Get(); }

  // Remembers what |key| in |store| held, so that Abort() can put it back.
  void RecordUndo(const std::shared_ptr<StoreData>& store, const IDBKey& key) {
    Undo undo;
    undo.store = store;
    undo.key = key;
    auto it = store->records.find(key);
    undo.existed = it != store->records.end();
    if (undo.existed) undo.record = it->second;
    undo_log.push_back(std::move(undo));
  }

  using ResultFactory = std::function<Local<Value>(Isolate*)>;

  // Completes |request| in a later task: sets its result and fires `success`
  // with this transaction active. |make_result| runs in the request's realm.
  void Succeed(Isolate* isolate, IDBRequest* request,
               ResultFactory make_result);
  // Fails |request| in a later task. The transaction aborts unless a
  // listener calls preventDefault().
  void Fail(Isolate* isolate, IDBRequest* request, const char* name,
            std::string message);

  // Commits if there is nothing left to wait for.
  void MaybeCommit(Isolate* isolate);
  void Abort(Isolate* isolate, Local<Value> reason);

  Member<Window> window;
  Member<IDBDatabase> db;
  Mode mode;
  std::vector<std::string> scope;
  // The open() request an upgrade transaction belongs to.
  Member<IDBRequest> open_request;
  TracedReference<Value> error;
  // New requests can be placed: during the task that created the
  // transaction and while one of its requests' events is dispatched.
  bool active = true;
  bool finished = false;
  bool aborted = false;
  int pending = 0;

 private:
  struct Undo {
    std::shared_ptr<StoreData> store;
    IDBKey key;
    bool existed = false;
    Record record;
  };
  std::vector<Undo> undo_log;
};

void IDBDatabase::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  visitor->Trace(window);
  visitor->Trace(upgrade_transaction);
}

void IDBTransaction::Succeed(Isolate* isolate, IDBRequest* request,
                             ResultFactory make_result) {
  ++pending;
  request->done = false;
  cppgc::Persistent<IDBTransaction> self(this);
  cppgc::Persistent<IDBRequest> keep(request);
  PostBackendTask(
      isolate, kRequestLatencyMs, [isolate, self, keep, make_result] {
        --self->pending;
        if (self->aborted || self->window->detached) return;
        HandleScope handle_scope(isolate);
        Context::Scope context_scope(self->window->context(isolate));
        keep->result.Reset(isolate, make_result(isolate));
        keep->error.Reset();
        keep->done = true;
        self->active = true;
        FireEvent(isolate, keep.Get(), NewEvent(isolate, "success"));
        self->active = false;
        self->MaybeCommit(isolate);
      });
}

void IDBTransaction::Fail(Isolate* isolate, IDBRequest* request,
                          const char* name, std::string message) {
  ++pending;
  request->done = false;
  cppgc::Persistent<IDBTransaction> self(this);
  cppgc::Persistent<IDBRequest> keep(request);
  PostBackendTask(
      isolate, kRequestLatencyMs, [isolate, self, keep, name, message] {
        --self->pending;
        if (self->aborted || self->window->detached) return;
        HandleScope handle_scope(isolate);
        Context::Scope context_scope(self->window->context(isolate));
        Local<Value> error = NewDOMException(isolate, name, message);
        keep->result.Reset();
        keep->error.Reset(isolate, error);
        keep->done = true;
        self->active = true;
        Event* event = NewEvent(isolate, "error", true, true);
        FireEvent(isolate, keep.Get(), event, {self.Get(), self->db.Get()});
        self->active = false;
        if (event->default_prevented) {
          self->MaybeCommit(isolate);
        } else {
          self->Abort(isolate, error);
        }
      });
}

void IDBTransaction::MaybeCommit(Isolate* isolate) {
  if (finished || pending > 0) return;
  finished = true;
  active = false;
  undo_log.clear();
  cppgc::Persistent<IDBTransaction> self(this);
  PostBackendTask(isolate, kRequestLatencyMs, [isolate, self] {
    if (self->window->detached) return;
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(self->window->context(isolate));
    FireEvent(isolate, self.Get(), NewEvent(isolate, "complete"));
    IDBRequest* request = self->open_request.Get();
    if (!request) return;
    // The upgrade is done: the open() request now succeeds.
    self->db->upgrade_transaction = nullptr;
    request->transaction = nullptr;
    request->done = true;
    FireEvent(isolate, request, NewEvent(isolate, "success"));
  });
}

void IDBTransaction::Abort(Isolate* isolate, Local<Value> reason) {
  if (finished) return;
  finished = true;
  aborted = true;
  active = false;
  for (auto it = undo_log.rbegin(); it != undo_log.rend(); ++it) {
    if (it->existed) {
      it->store->InsertRecord(it->key, std::move(it->record));
    } else {
      it->store->RemoveRecord(it->key);
    }
  }
  undo_log.clear();
  if (!reason.IsEmpty()) error.Reset(isolate, reason);
  cppgc::Persistent<IDBTransaction> self(this);
  PostTask(isolate, [isolate, self] {
    if (self->window->detached) return;
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(self->window->context(isolate));
    FireEvent(isolate, self.Get(), NewEvent(isolate, "abort", true),
              {self->db.Get()});
    IDBRequest* request = self->open_request.Get();
    if (!request) return;
    self->db->upgrade_transaction = nullptr;
    self->db->closed = true;
    request->transaction = nullptr;
    request->result.Reset();
    request->error.Reset(
        isolate, NewDOMException(isolate, "AbortError",
                                 "The upgrade transaction was aborted."));
    request->done = true;
    FireEvent(isolate, request, NewEvent(isolate, "error", true, true));
  });
}

class IDBObjectStore final : public ScriptWrappable {
 public:
  IDBObjectStore(IDBTransaction* transaction, std::shared_ptr<StoreData> data)
      : ScriptWrappable(Interface::kIDBObjectStore),
        transaction(transaction),
        data(std::move(data)) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(transaction);
  }
  Member<IDBTransaction> transaction;
  std::shared_ptr<StoreData> data;
};

class IDBIndex final : public ScriptWrappable {
 public:
  IDBIndex(IDBObjectStore* store, std::string name)
      : ScriptWrappable(Interface::kIDBIndex),
        store(store),
        name(std::move(name)) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(store);
  }
  // Null once the index has been deleted.
  IndexData* data() const { return store->data->FindIndex(name); }
  Member<IDBObjectStore> store;
  std::string name;
};

class IDBCursor final : public ScriptWrappable {
 public:
  enum Direction : uint8_t { kNext, kNextUnique, kPrev, kPrevUnique };

  IDBCursor(Interface interface, IDBRequest* request, IDBObjectStore* store,
            IDBIndex* index, KeyRange range, Direction direction)
      : ScriptWrappable(interface),
        request(request),
        store(store),
        index(index),
        range(std::move(range)),
        direction(direction) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(request);
    visitor->Trace(store);
    visitor->Trace(index);
    visitor->Trace(key_value);
    visitor->Trace(primary_key_value);
    visitor->Trace(value);
  }
  bool key_only() const { return interface() == Interface::kIDBCursor; }
  bool forward() const {
    return direction == kNext || direction == kNextUnique;
  }
  bool unique() const {
    return direction == kNextUnique || direction == kPrevUnique;
  }

  // Moves to the next record in iteration order whose key is not before
  // |target| (if valid). Returns false at the end of the range.
  bool Step(const IDBKey& target);

  Member<IDBRequest> request;
  Member<IDBObjectStore> store;
  // Null for a cursor over the object store itself.
  Member<IDBIndex> index;
  KeyRange range;
  Direction direction;
  // The record the cursor is at. Valid while |got_value|.
  IDBKey key;
  IDBKey primary_key;
  bool started = false;
  bool got_value = false;
  TracedReference<Value> key_value;
  TracedReference<Value> primary_key_value;
  TracedReference<Value> value;
};

// Probes that sort before and after every real key.
IDBKey MinKey() { return IDBKey(); }
IDBKey MaxKey() {
  IDBKey key;
  key.type = IDBKey::kMax;
  return key;
}

bool IDBCursor::Step(const IDBKey& target) {
  using Entry = std::pair<IDBKey, IDBKey>;
  IndexData::EntryLess less;
  IndexData* index_data = index ? index->data() : nullptr;
  if (index && !index_data) return false;
  const auto& records = store->data->records;
  // An object store is iterated like an index whose entries are (key, key).
  Entry found;
  bool have = false;
  if (forward()) {
    Entry from;  // The first entry at or after |from| is the next one.
    if (!started) {
      from = range.lower.valid()
                 ? Entry(range.lower, range.lower_open ? MaxKey() : MinKey())
                 : Entry(MinKey(), MinKey());
    } else {
      from = Entry(key, unique() || !index ? MaxKey() : primary_key);
      if (index && !unique()) {
        // Strictly after (key, primary_key).
        from.second = primary_key;
      }
    }
    bool strictly_after = started && index && !unique();
    if (target.valid() && less(from, Entry(target, MinKey()))) {
      from = Entry(target, MinKey());
      strictly_after = false;
    }
    if (index_data) {
      auto it = strictly_after ? index_data->entries.upper_bound(from)
                               : index_data->entries.lower_bound(from);
      if (it != index_data->entries.end()) {
        found = *it;
        have = true;
      }
    } else {
      // |from.second| only says whether the key itself is still wanted.
      auto it = from.second.type == IDBKey::kMax
                    ? records.upper_bound(from.first)
                    : records.lower_bound(from.first);
      if (it != records.end()) {
        found = Entry(it->first, it->first);
        have = true;
      }
    }
  } else {
    Entry before;  // The last entry before |before| is the next one.
    if (!started) {
      before = range.upper.valid()
                   ? Entry(range.upper, range.upper_open ? MinKey() : MaxKey())
                   : Entry(MaxKey(), MaxKey());
    } else {
      before = Entry(key, unique() || !index ? MinKey() : primary_key);
    }
    if (target.valid() && less(Entry(target, MaxKey()), before)) {
      before = Entry(target, MaxKey());
    }
    if (index_data) {
      auto it = index_data->entries.lower_bound(before);
      if (it != index_data->entries.begin()) {
        --it;
        if (unique()) {
          // The first entry with that key.
          it = index_data->entries.lower_bound(Entry(it->first, MinKey()));
        }
        found = *it;
        have = true;
      }
    } else {
      auto it = before.second.type == IDBKey::kMax
                    ? records.upper_bound(before.first)
                    : records.lower_bound(before.first);
      if (it != records.begin()) {
        --it;
        found = Entry(it->first, it->first);
        have = true;
      }
    }
  }
  started = true;
  if (!have || !range.Includes(found.first)) {
    got_value = false;
    return false;
  }
  key = std::move(found.first);
  primary_key = std::move(found.second);
  got_value = true;
  return true;
}

// --- Operations --------------------------------------------------------------

IDBRequest* NewRequest(Isolate* isolate, IDBTransaction* transaction,
                       ScriptWrappable* source) {
  IDBRequest* request = MakeGarbageCollected<IDBRequest>(
      isolate, Interface::kIDBRequest, transaction->window.Get());
  request->transaction = transaction;
  request->source = source;
  return request;
}

// Throws and returns false unless a request can be placed against
// |transaction| now.
bool CheckActive(Isolate* isolate, IDBTransaction* transaction,
                 bool for_writing) {
  if (transaction->finished || !transaction->active) {
    ThrowDOMException(isolate, "TransactionInactiveError",
                      transaction->finished ? "The transaction has finished."
                                            : "The transaction is not active.");
    return false;
  }
  if (for_writing && transaction->mode == IDBTransaction::kReadOnly) {
    ThrowDOMException(isolate, "ReadOnlyError",
                      "The transaction is read-only.");
    return false;
  }
  return true;
}

// Converts a query argument (a key, an IDBKeyRange, or nothing) to a range.
bool ToKeyRange(Isolate* isolate, Local<Value> value, bool required,
                KeyRange* out) {
  if (value->IsNullOrUndefined()) {
    if (!required) return true;
    ThrowDOMException(isolate, "DataError", "No key or key range specified.");
    return false;
  }
  if (IDBKeyRangeObject* range =
          To<IDBKeyRangeObject>(isolate, value, Interface::kIDBKeyRange)) {
    *out = range->range;
    return true;
  }
  IDBKey key = KeyFromValue(isolate, value);
  if (isolate->HasPendingException()) return false;
  if (!key.valid()) {
    ThrowDOMException(isolate, "DataError",
                      "The parameter is not a valid key.");
    return false;
  }
  out->lower = key;
  out->upper = std::move(key);
  return true;
}

Local<Value> Arg(const CallbackInfo& info, int index) {
  return info.Length() > index ? info[index]
                               : Undefined(info.GetIsolate()).As<Value>();
}

// The keys a value has in |index|.
std::vector<IDBKey> IndexKeysFor(Isolate* isolate, Local<Value> value,
                                 const IndexData& index) {
  std::vector<IDBKey> keys;
  IDBKey key = ExtractKey(isolate, value, index.key_path);
  if (!key.valid()) return keys;
  if (index.multi_entry && key.type == IDBKey::kArray) {
    for (IDBKey& element : key.array) {
      bool duplicate = false;
      for (const IDBKey& existing : keys) {
        if (CompareKeys(existing, element) == 0) duplicate = true;
      }
      if (!duplicate) keys.push_back(std::move(element));
    }
  } else {
    keys.push_back(std::move(key));
  }
  return keys;
}

// put() and add(), and IDBCursor.update() with |cursor_key|.
void PutRecord(const CallbackInfo& info, IDBObjectStore* store,
               ScriptWrappable* source, Local<Value> value,
               Local<Value> key_argument, bool no_overwrite,
               const IDBKey* cursor_key) {
  Isolate* isolate = info.GetIsolate();
  IDBTransaction* transaction = store->transaction.Get();
  if (!CheckActive(isolate, transaction, true)) return;
  StoreData* data = store->data.get();
  const bool inline_keys = !data->key_path.is_null;
  const bool has_key_argument = !key_argument->IsUndefined();

  IDBKey key;
  if (cursor_key) {
    key = *cursor_key;
  } else if (has_key_argument) {
    if (inline_keys) {
      ThrowDOMException(isolate, "DataError",
                        "The object store uses in-line keys and the key "
                        "parameter was provided.");
      return;
    }
    key = KeyFromValue(isolate, key_argument);
    if (isolate->HasPendingException()) return;
    if (!key.valid()) {
      ThrowDOMException(isolate, "DataError",
                        "The parameter is not a valid key.");
      return;
    }
    if (!CheckActive(isolate, transaction, true)) return;
  } else if (!inline_keys && !data->auto_increment) {
    ThrowDOMException(isolate, "DataError",
                      "The object store uses out-of-line keys and has no key "
                      "generator and the key parameter was not provided.");
    return;
  }

  Record record;
  if (!Serialize(isolate, value, &record.value)) return;
  if (!CheckActive(isolate, transaction, true)) return;
  // Keys are read from the stored clone, not from the caller's object, as
  // in a browser. The clone is only materialized if something needs it.
  Local<Value> clone;
  auto materialize = [&] {
    if (clone.IsEmpty()) clone = Deserialize(isolate, record.value);
    return clone;
  };

  if (inline_keys) {
    IDBKey extracted = ExtractKey(isolate, materialize(), data->key_path);
    if (cursor_key) {
      if (!extracted.valid() || CompareKeys(extracted, key) != 0) {
        ThrowDOMException(isolate, "DataError",
                          "The effective object store of this cursor uses "
                          "in-line keys and evaluating the key path of the "
                          "value parameter results in a different value than "
                          "the cursor's effective key.");
        return;
      }
    } else if (extracted.valid()) {
      key = std::move(extracted);
    } else if (!data->auto_increment) {
      ThrowDOMException(isolate, "DataError",
                        "Evaluating the object store's key path did not "
                        "yield a value.");
      return;
    }
  }
  if (!key.valid()) {
    // Key generator.
    key.type = IDBKey::kNumber;
    key.number = data->key_generator++;
    if (inline_keys && !data->key_path.is_array) {
      InjectKey(isolate, materialize(), data->key_path.paths[0], key);
      if (!Serialize(isolate, clone, &record.value)) return;
    }
  } else if (data->auto_increment && key.type == IDBKey::kNumber &&
             key.number >= data->key_generator) {
    data->key_generator = std::floor(key.number) + 1;
  }

  for (const auto& index : data->indexes) {
    record.index_keys.push_back(IndexKeysFor(isolate, materialize(), *index));
  }

  IDBRequest* request = NewRequest(isolate, transaction, source);
  Return(info, request);

  if (no_overwrite && data->records.count(key)) {
    transaction->Fail(isolate, request, "ConstraintError",
                      "Key already exists in the object store.");
    return;
  }
  for (size_t i = 0; i < data->indexes.size(); ++i) {
    const IndexData& index = *data->indexes[i];
    if (!index.unique) continue;
    for (const IDBKey& index_key : record.index_keys[i]) {
      auto it = index.entries.lower_bound({index_key, MinKey()});
      if (it != index.entries.end() && CompareKeys(it->first, index_key) == 0 &&
          CompareKeys(it->second, key) != 0) {
        transaction->Fail(isolate, request, "ConstraintError",
                          "Unable to add key to index '" + index.name +
                              "': at least one key does not satisfy the "
                              "uniqueness requirements.");
        return;
      }
    }
  }
  transaction->RecordUndo(store->data, key);
  data->InsertRecord(key, std::move(record));
  transaction->Succeed(isolate, request, [key](Isolate* isolate) {
    return KeyToValue(isolate, key);
  });
}

// What a read returns for each matching record.
enum class ReadKind { kValue, kPrimaryKey };

// The primary keys of the records |range| selects, through |index| if given,
// in key order, at most |limit| of them.
std::vector<IDBKey> KeysInRange(const StoreData& store, const IndexData* index,
                                const KeyRange& range, size_t limit) {
  std::vector<IDBKey> keys;
  if (index) {
    auto it = range.lower.valid()
                  ? index->entries.lower_bound({range.lower, MinKey()})
                  : index->entries.begin();
    for (; it != index->entries.end() && keys.size() < limit; ++it) {
      if (range.upper.valid() && CompareKeys(it->first, range.upper) > 0) break;
      if (range.Includes(it->first)) keys.push_back(it->second);
    }
  } else {
    auto it = range.lower.valid() ? store.records.lower_bound(range.lower)
                                  : store.records.begin();
    for (; it != store.records.end() && keys.size() < limit; ++it) {
      if (range.upper.valid() && CompareKeys(it->first, range.upper) > 0) break;
      if (range.Includes(it->first)) keys.push_back(it->first);
    }
  }
  return keys;
}

constexpr size_t kNoLimit = ~size_t{0};

// get(), getKey(), getAll() and getAllKeys() of object stores and indexes.
void ReadRecords(const CallbackInfo& info, IDBObjectStore* store,
                 IDBIndex* index, ReadKind kind, bool all) {
  Isolate* isolate = info.GetIsolate();
  IDBTransaction* transaction = store->transaction.Get();
  if (!CheckActive(isolate, transaction, false)) return;
  IndexData* index_data = index ? index->data() : nullptr;
  if (index && !index_data) {
    ThrowDOMException(isolate, "InvalidStateError",
                      "The index has been deleted.");
    return;
  }
  KeyRange range;
  if (!ToKeyRange(isolate, Arg(info, 0), !all, &range)) return;
  size_t limit = all ? kNoLimit : 1;
  if (all && !Arg(info, 1)->IsUndefined()) {
    double count = 0;
    if (!ToNumber(info, info[1], &count)) return;
    if (count >= 1) limit = static_cast<size_t>(count);
  }
  std::vector<IDBKey> keys =
      KeysInRange(*store->data, index_data, range, limit);
  // Copy what the result needs now; it is delivered later.
  auto values = std::make_shared<std::vector<std::vector<uint8_t>>>();
  if (kind == ReadKind::kValue) {
    for (const IDBKey& key : keys) {
      values->push_back(store->data->records.find(key)->second.value);
    }
  }
  auto shared_keys = std::make_shared<std::vector<IDBKey>>(std::move(keys));
  ScriptWrappable* source = index ? static_cast<ScriptWrappable*>(index)
                                  : static_cast<ScriptWrappable*>(store);
  IDBRequest* request = NewRequest(isolate, transaction, source);
  Return(info, request);
  transaction->Succeed(
      isolate, request,
      [kind, all, values, shared_keys](Isolate* isolate) -> Local<Value> {
        auto item = [&](size_t i) -> Local<Value> {
          return kind == ReadKind::kValue
                     ? Deserialize(isolate, (*values)[i])
                     : KeyToValue(isolate, (*shared_keys)[i]);
        };
        if (!all) {
          return shared_keys->empty() ? Undefined(isolate).As<Value>()
                                      : item(0);
        }
        LocalVector<Value> items(isolate);
        items.reserve(shared_keys->size());
        for (size_t i = 0; i < shared_keys->size(); ++i) {
          items.push_back(item(i));
        }
        return Array::New(isolate, items.data(), items.size());
      });
}

void CountRecords(const CallbackInfo& info, IDBObjectStore* store,
                  IDBIndex* index) {
  Isolate* isolate = info.GetIsolate();
  IDBTransaction* transaction = store->transaction.Get();
  if (!CheckActive(isolate, transaction, false)) return;
  KeyRange range;
  if (!ToKeyRange(isolate, Arg(info, 0), false, &range)) return;
  size_t count = KeysInRange(*store->data, index ? index->data() : nullptr,
                             range, kNoLimit)
                     .size();
  ScriptWrappable* source = index ? static_cast<ScriptWrappable*>(index)
                                  : static_cast<ScriptWrappable*>(store);
  IDBRequest* request = NewRequest(isolate, transaction, source);
  Return(info, request);
  transaction->Succeed(isolate, request, [count](Isolate* isolate) {
    return Number::New(isolate, static_cast<double>(count)).As<Value>();
  });
}

// --- Cursors -----------------------------------------------------------------

// Completes the cursor's request with the cursor at its current record, or
// with null at the end of the range.
void DeliverCursor(Isolate* isolate, IDBCursor* cursor) {
  IDBTransaction* transaction = cursor->store->transaction.Get();
  cppgc::Persistent<IDBCursor> keep(cursor);
  auto value = std::make_shared<std::vector<uint8_t>>();
  const bool at_record = cursor->got_value;
  if (at_record && !cursor->key_only()) {
    auto it = cursor->store->data->records.find(cursor->primary_key);
    if (it != cursor->store->data->records.end()) *value = it->second.value;
  }
  transaction->Succeed(
      isolate, cursor->request.Get(),
      [keep, value, at_record](Isolate* isolate) -> Local<Value> {
        if (!at_record) return Null(isolate);
        keep->key_value.Reset(isolate, KeyToValue(isolate, keep->key));
        keep->primary_key_value.Reset(isolate,
                                      KeyToValue(isolate, keep->primary_key));
        if (!keep->key_only()) {
          keep->value.Reset(isolate, Deserialize(isolate, *value));
        }
        return ToV8(isolate, keep.Get());
      });
}

bool ParseDirection(const CallbackInfo& info, Local<Value> value,
                    IDBCursor::Direction* out) {
  *out = IDBCursor::kNext;
  if (value->IsUndefined()) return true;
  std::string text;
  if (!ToStdString(info, value, &text)) return false;
  static const char* const kNames[] = {"next", "nextunique", "prev",
                                       "prevunique"};
  for (int i = 0; i < 4; ++i) {
    if (text == kNames[i]) {
      *out = static_cast<IDBCursor::Direction>(i);
      return true;
    }
  }
  ThrowTypeError(info.GetIsolate(), "The provided value '" + text +
                                        "' is not a valid enum value of type "
                                        "IDBCursorDirection.");
  return false;
}

void OpenCursor(const CallbackInfo& info, IDBObjectStore* store,
                IDBIndex* index, bool key_only) {
  Isolate* isolate = info.GetIsolate();
  IDBTransaction* transaction = store->transaction.Get();
  if (!CheckActive(isolate, transaction, false)) return;
  KeyRange range;
  IDBCursor::Direction direction;
  if (!ToKeyRange(isolate, Arg(info, 0), false, &range) ||
      !ParseDirection(info, Arg(info, 1), &direction)) {
    return;
  }
  ScriptWrappable* source = index ? static_cast<ScriptWrappable*>(index)
                                  : static_cast<ScriptWrappable*>(store);
  IDBRequest* request = NewRequest(isolate, transaction, source);
  IDBCursor* cursor = MakeGarbageCollected<IDBCursor>(
      isolate,
      key_only ? Interface::kIDBCursor : Interface::kIDBCursorWithValue,
      request, store, index, std::move(range), direction);
  cursor->Step(IDBKey());
  DeliverCursor(isolate, cursor);
  Return(info, request);
}

// Checks shared by continue(), advance(), update() and delete().
bool CheckCursor(Isolate* isolate, IDBCursor* cursor, bool for_writing) {
  if (!CheckActive(isolate, cursor->store->transaction.Get(), for_writing)) {
    return false;
  }
  if (!cursor->got_value || !cursor->request->done) {
    ThrowDOMException(isolate, "InvalidStateError",
                      "The cursor is being iterated or has iterated past "
                      "its end.");
    return false;
  }
  return true;
}

void DeleteRange(Isolate* isolate, IDBTransaction* transaction,
                 const std::shared_ptr<StoreData>& store,
                 const KeyRange& range) {
  for (const IDBKey& key : KeysInRange(*store, nullptr, range, kNoLimit)) {
    transaction->RecordUndo(store, key);
    store->RemoveRecord(key);
  }
}

// --- Bindings ----------------------------------------------------------------

Local<Value> UndefinedResult(Isolate* isolate) { return Undefined(isolate); }

Intercepted DOMStringListGetter(uint32_t index,
                                const PropertyCallbackInfo<Value>& info) {
  DOMStringList* list =
      static_cast<DOMStringList*>(static_cast<ScriptWrappable*>(
          Object::Unwrap<kWrapperTag, Object::Wrappable>(info.GetIsolate(),
                                                         info.Holder())));
  if (index >= list->items.size()) return Intercepted::kNo;
  info.GetReturnValue().Set(
      ToV8StringFromUtf8(info.GetIsolate(), list->items[index]));
  return Intercepted::kYes;
}

void InstallStringListBindings(Isolate* isolate) {
  const Interface kList = Interface::kDOMStringList;
  InterfaceTemplate(isolate, kList)
      ->InstanceTemplate()
      ->SetHandler(IndexedPropertyHandlerConfiguration(DOMStringListGetter));
  DefineAccessor(isolate, kList, "length", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        static_cast<uint32_t>(Receiver<DOMStringList>(info)->items.size()));
  });
  DefineMethod(
      isolate, kList, "item",
      [](const CallbackInfo& info) {
        DOMStringList* list = Receiver<DOMStringList>(info);
        double index = 0;
        if (info.Length() > 0 && !ToNumber(info, info[0], &index)) return;
        if (index < 0 || index >= list->items.size()) {
          info.GetReturnValue().SetNull();
          return;
        }
        info.GetReturnValue().Set(ToV8StringFromUtf8(
            info.GetIsolate(), list->items[static_cast<size_t>(index)]));
      },
      1);
  DefineMethod(
      isolate, kList, "contains",
      [](const CallbackInfo& info) {
        std::string name;
        if (!RequireArguments(info, 1) || !ToStdString(info, info[0], &name)) {
          return;
        }
        const auto& items = Receiver<DOMStringList>(info)->items;
        info.GetReturnValue().Set(std::find(items.begin(), items.end(), name) !=
                                  items.end());
      },
      1);
}

template <typename T>
void ReturnNames(const CallbackInfo& info, const T& named) {
  std::vector<std::string> names;
  for (const auto& entry : named) names.push_back(entry->name);
  Return(info, MakeGarbageCollected<DOMStringList>(info.GetIsolate(),
                                                   std::move(names)));
}

// IDBKeyRange.only(), lowerBound(), upperBound() and bound().
void MakeKeyRange(const CallbackInfo& info, bool has_lower, bool has_upper) {
  Isolate* isolate = info.GetIsolate();
  KeyRange range;
  int argument = 0;
  auto next_key = [&](IDBKey* out) {
    *out = KeyFromValue(isolate, Arg(info, argument++));
    if (out->valid()) return true;
    ThrowDOMException(isolate, "DataError",
                      "The parameter is not a valid key.");
    return false;
  };
  if (has_lower && !next_key(&range.lower)) return;
  if (has_upper && !next_key(&range.upper)) return;
  if (has_lower)
    range.lower_open = Arg(info, argument++)->BooleanValue(isolate);
  if (has_upper)
    range.upper_open = Arg(info, argument++)->BooleanValue(isolate);
  if (has_lower && has_upper) {
    int c = CompareKeys(range.lower, range.upper);
    if (c > 0 || (c == 0 && (range.lower_open || range.upper_open))) {
      ThrowDOMException(isolate, "DataError",
                        "The lower key is greater than the upper key.");
      return;
    }
  }
  Return(info,
         MakeGarbageCollected<IDBKeyRangeObject>(isolate, std::move(range)));
}

void InstallKeyRangeBindings(Isolate* isolate) {
  const Interface kRange = Interface::kIDBKeyRange;
  DefineStaticMethod(
      isolate, kRange, "only",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        KeyRange range;
        range.lower = KeyFromValue(isolate, Arg(info, 0));
        if (!range.lower.valid()) {
          ThrowDOMException(isolate, "DataError",
                            "The parameter is not a valid key.");
          return;
        }
        range.upper = range.lower;
        Return(info, MakeGarbageCollected<IDBKeyRangeObject>(isolate,
                                                             std::move(range)));
      },
      1);
  DefineStaticMethod(
      isolate, kRange, "lowerBound",
      [](const CallbackInfo& info) { MakeKeyRange(info, true, false); }, 1);
  DefineStaticMethod(
      isolate, kRange, "upperBound",
      [](const CallbackInfo& info) { MakeKeyRange(info, false, true); }, 1);
  DefineStaticMethod(
      isolate, kRange, "bound",
      [](const CallbackInfo& info) { MakeKeyRange(info, true, true); }, 2);
  DefineAccessor(isolate, kRange, "lower", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(KeyToValue(
        info.GetIsolate(), Receiver<IDBKeyRangeObject>(info)->range.lower));
  });
  DefineAccessor(isolate, kRange, "upper", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(KeyToValue(
        info.GetIsolate(), Receiver<IDBKeyRangeObject>(info)->range.upper));
  });
  DefineAccessor(isolate, kRange, "lowerOpen", [](const CallbackInfo& info) {
    const KeyRange& range = Receiver<IDBKeyRangeObject>(info)->range;
    info.GetReturnValue().Set(range.lower_open || !range.lower.valid());
  });
  DefineAccessor(isolate, kRange, "upperOpen", [](const CallbackInfo& info) {
    const KeyRange& range = Receiver<IDBKeyRangeObject>(info)->range;
    info.GetReturnValue().Set(range.upper_open || !range.upper.valid());
  });
  DefineMethod(
      isolate, kRange, "includes",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        IDBKey key = KeyFromValue(isolate, Arg(info, 0));
        if (!key.valid()) {
          ThrowDOMException(isolate, "DataError",
                            "The parameter is not a valid key.");
          return;
        }
        info.GetReturnValue().Set(
            Receiver<IDBKeyRangeObject>(info)->range.Includes(key));
      },
      1);
}

// `onfoo` attributes for the given event types.
void DefineEventHandlers(Isolate* isolate, Interface interface,
                         std::initializer_list<const char*> types) {
  for (const char* type : types) {
    std::string property = std::string("on") + type;
    DefineAccessor(
        isolate, interface, property.c_str(),
        [](const CallbackInfo& info) {
          Isolate* isolate = info.GetIsolate();
          info.GetReturnValue().Set(
              Receiver<EventTarget>(info)->GetAttributeListener(
                  isolate,
                  ToAtomicString(isolate, info.Data().As<v8::String>())));
        },
        [](const CallbackInfo& info) {
          Isolate* isolate = info.GetIsolate();
          Receiver<EventTarget>(info)->SetAttributeListener(
              isolate, ToAtomicString(isolate, info.Data().As<v8::String>()),
              Arg(info, 0));
        },
        V8Name(isolate, type));
  }
}

void InstallRequestBindings(Isolate* isolate) {
  const Interface kRequest = Interface::kIDBRequest;
  DefineAccessor(isolate, kRequest, "result", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    IDBRequest* request = Receiver<IDBRequest>(info);
    if (!request->done) {
      ThrowDOMException(isolate, "InvalidStateError",
                        "The request has not finished.");
    } else if (!request->result.IsEmpty()) {
      info.GetReturnValue().Set(request->result.Get(isolate));
    }
  });
  DefineAccessor(isolate, kRequest, "error", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    IDBRequest* request = Receiver<IDBRequest>(info);
    if (!request->done) {
      ThrowDOMException(isolate, "InvalidStateError",
                        "The request has not finished.");
    } else if (request->error.IsEmpty()) {
      info.GetReturnValue().SetNull();
    } else {
      info.GetReturnValue().Set(request->error.Get(isolate));
    }
  });
  DefineAccessor(isolate, kRequest, "source", [](const CallbackInfo& info) {
    Return(info, Receiver<IDBRequest>(info)->source.Get());
  });
  DefineAccessor(isolate, kRequest, "transaction",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<IDBRequest>(info)->transaction.Get());
                 });
  DefineAccessor(isolate, kRequest, "readyState", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        V8Name(info.GetIsolate(),
               Receiver<IDBRequest>(info)->done ? "done" : "pending"));
  });
  DefineEventHandlers(isolate, kRequest, {"success", "error"});
  DefineEventHandlers(isolate, Interface::kIDBOpenDBRequest,
                      {"blocked", "upgradeneeded"});
}

IDBDatabase* DatabaseOf(const CallbackInfo& info) {
  return Receiver<IDBDatabase>(info);
}

// db.transaction(names, mode).
CALLBACK(DatabaseTransaction) {
  Isolate* isolate = info.GetIsolate();
  IDBDatabase* db = DatabaseOf(info);
  if (!RequireArguments(info, 1)) return;
  if (db->closed || db->upgrade_transaction) {
    ThrowDOMException(isolate, "InvalidStateError",
                      db->closed ? "The database connection is closing."
                                 : "A version change transaction is running.");
    return;
  }
  std::vector<std::string> names;
  Local<Context> context = isolate->GetCurrentContext();
  if (DOMStringList* list =
          To<DOMStringList>(isolate, info[0], Interface::kDOMStringList)) {
    names = list->items;
  } else if (info[0]->IsObject() && !info[0]->IsString()) {
    // Any iterable of names; arrays are what pages pass.
    Local<Array> array;
    if (info[0]->IsArray()) {
      array = info[0].As<Array>();
    } else {
      ThrowTypeError(isolate, "The storeNames parameter is not a sequence.");
      return;
    }
    for (uint32_t i = 0; i < array->Length(); ++i) {
      Local<Value> entry;
      std::string name;
      if (!array->Get(context, i).ToLocal(&entry) ||
          !ToStdString(info, entry, &name)) {
        return;
      }
      names.push_back(std::move(name));
    }
  } else {
    std::string name;
    if (!ToStdString(info, info[0], &name)) return;
    names.push_back(std::move(name));
  }
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  if (names.empty()) {
    ThrowDOMException(isolate, "InvalidAccessError",
                      "The storeNames parameter was empty.");
    return;
  }
  for (const std::string& name : names) {
    if (!db->data->FindStore(name)) {
      ThrowDOMException(isolate, "NotFoundError",
                        "One of the specified object stores was not found.");
      return;
    }
  }
  IDBTransaction::Mode mode = IDBTransaction::kReadOnly;
  if (!Arg(info, 1)->IsUndefined()) {
    std::string text;
    if (!ToStdString(info, info[1], &text)) return;
    if (text == "readwrite") {
      mode = IDBTransaction::kReadWrite;
    } else if (text != "readonly") {
      ThrowTypeError(isolate, "The provided value '" + text +
                                  "' is not a valid enum value of type "
                                  "IDBTransactionMode.");
      return;
    }
  }
  IDBTransaction* transaction = MakeGarbageCollected<IDBTransaction>(
      isolate, db->window.Get(), db, mode, std::move(names));
  // Once this task is over the transaction can only be used from its own
  // requests' callbacks; with nothing requested, it commits.
  cppgc::Persistent<IDBTransaction> keep(transaction);
  PostTask(isolate, [isolate, keep] {
    keep->active = false;
    keep->MaybeCommit(isolate);
  });
  Return(info, transaction);
}

CALLBACK(DatabaseCreateObjectStore) {
  Isolate* isolate = info.GetIsolate();
  IDBDatabase* db = DatabaseOf(info);
  if (!RequireArguments(info, 1)) return;
  IDBTransaction* transaction = db->upgrade_transaction.Get();
  if (!transaction) {
    ThrowDOMException(isolate, "InvalidStateError",
                      "The database is not running a version change "
                      "transaction.");
    return;
  }
  if (!CheckActive(isolate, transaction, true)) return;
  auto store = std::make_shared<StoreData>();
  if (!ToStdString(info, info[0], &store->name)) return;
  if (Arg(info, 1)->IsObject()) {
    Local<Object> options = info[1].As<Object>();
    Local<Value> key_path, auto_increment;
    if (!GetProperty(isolate, options, "keyPath", &key_path) ||
        !GetProperty(isolate, options, "autoIncrement", &auto_increment) ||
        !ParseKeyPath(info, key_path, &store->key_path)) {
      return;
    }
    store->auto_increment = auto_increment->BooleanValue(isolate);
  }
  if (db->data->FindStore(store->name)) {
    ThrowDOMException(isolate, "ConstraintError",
                      "An object store with the specified name already "
                      "exists.");
    return;
  }
  db->data->stores.push_back(store);
  transaction->scope.push_back(store->name);
  Return(info, MakeGarbageCollected<IDBObjectStore>(isolate, transaction,
                                                    std::move(store)));
}

CALLBACK(DatabaseDeleteObjectStore) {
  Isolate* isolate = info.GetIsolate();
  IDBDatabase* db = DatabaseOf(info);
  std::string name;
  if (!RequireArguments(info, 1) || !ToStdString(info, info[0], &name)) return;
  IDBTransaction* transaction = db->upgrade_transaction.Get();
  if (!transaction) {
    ThrowDOMException(isolate, "InvalidStateError",
                      "The database is not running a version change "
                      "transaction.");
    return;
  }
  if (!CheckActive(isolate, transaction, true)) return;
  auto& stores = db->data->stores;
  auto it = std::find_if(stores.begin(), stores.end(), [&](const auto& store) {
    return store->name == name;
  });
  if (it == stores.end()) {
    ThrowDOMException(isolate, "NotFoundError",
                      "The specified object store was not found.");
    return;
  }
  stores.erase(it);
}

void InstallDatabaseBindings(Isolate* isolate) {
  const Interface kDatabase = Interface::kIDBDatabase;
  DefineAccessor(isolate, kDatabase, "name", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        ToV8StringFromUtf8(info.GetIsolate(), DatabaseOf(info)->data->name));
  });
  DefineAccessor(isolate, kDatabase, "version", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(DatabaseOf(info)->data->version);
  });
  DefineAccessor(isolate, kDatabase, "objectStoreNames",
                 [](const CallbackInfo& info) {
                   ReturnNames(info, DatabaseOf(info)->data->stores);
                 });
  DefineMethod(isolate, kDatabase, "transaction", DatabaseTransaction, 1);
  DefineMethod(isolate, kDatabase, "createObjectStore",
               DatabaseCreateObjectStore, 1);
  DefineMethod(isolate, kDatabase, "deleteObjectStore",
               DatabaseDeleteObjectStore, 1);
  DefineMethod(isolate, kDatabase, "close", [](const CallbackInfo& info) {
    DatabaseOf(info)->closed = true;
  });
  DefineEventHandlers(isolate, kDatabase,
                      {"abort", "close", "error", "versionchange"});
}

void InstallTransactionBindings(Isolate* isolate) {
  const Interface kTransaction = Interface::kIDBTransaction;
  DefineAccessor(isolate, kTransaction, "db", [](const CallbackInfo& info) {
    Return(info, Receiver<IDBTransaction>(info)->db.Get());
  });
  DefineAccessor(isolate, kTransaction, "mode", [](const CallbackInfo& info) {
    static const char* const kNames[] = {"readonly", "readwrite",
                                         "versionchange"};
    info.GetReturnValue().Set(V8Name(
        info.GetIsolate(), kNames[Receiver<IDBTransaction>(info)->mode]));
  });
  DefineAccessor(
      isolate, kTransaction, "durability", [](const CallbackInfo& info) {
        info.GetReturnValue().Set(V8Name(info.GetIsolate(), "default"));
      });
  DefineAccessor(isolate, kTransaction, "error", [](const CallbackInfo& info) {
    IDBTransaction* transaction = Receiver<IDBTransaction>(info);
    if (transaction->error.IsEmpty()) {
      info.GetReturnValue().SetNull();
    } else {
      info.GetReturnValue().Set(transaction->error.Get(info.GetIsolate()));
    }
  });
  DefineAccessor(
      isolate, kTransaction, "objectStoreNames", [](const CallbackInfo& info) {
        Return(info,
               MakeGarbageCollected<DOMStringList>(
                   info.GetIsolate(), Receiver<IDBTransaction>(info)->scope));
      });
  DefineMethod(
      isolate, kTransaction, "objectStore",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        IDBTransaction* transaction = Receiver<IDBTransaction>(info);
        std::string name;
        if (!RequireArguments(info, 1) || !ToStdString(info, info[0], &name)) {
          return;
        }
        if (transaction->finished) {
          ThrowDOMException(isolate, "InvalidStateError",
                            "The transaction has finished.");
          return;
        }
        std::shared_ptr<StoreData> store =
            transaction->db->data->FindStore(name);
        const auto& scope = transaction->scope;
        if (!store ||
            std::find(scope.begin(), scope.end(), name) == scope.end()) {
          ThrowDOMException(isolate, "NotFoundError",
                            "The specified object store was not found.");
          return;
        }
        Return(info, MakeGarbageCollected<IDBObjectStore>(isolate, transaction,
                                                          std::move(store)));
      },
      1);
  DefineMethod(isolate, kTransaction, "commit", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    IDBTransaction* transaction = Receiver<IDBTransaction>(info);
    if (transaction->finished || !transaction->active) {
      ThrowDOMException(isolate, "InvalidStateError",
                        "The transaction is not active.");
      return;
    }
    // No further requests; it completes when the placed ones have.
    transaction->active = false;
    transaction->MaybeCommit(isolate);
  });
  DefineMethod(isolate, kTransaction, "abort", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    IDBTransaction* transaction = Receiver<IDBTransaction>(info);
    if (transaction->finished) {
      ThrowDOMException(isolate, "InvalidStateError",
                        "The transaction has finished.");
      return;
    }
    transaction->Abort(isolate, Local<Value>());
  });
  DefineEventHandlers(isolate, kTransaction, {"abort", "complete", "error"});
}

IDBObjectStore* StoreOf(const CallbackInfo& info) {
  return Receiver<IDBObjectStore>(info);
}

CALLBACK(StoreDelete) {
  Isolate* isolate = info.GetIsolate();
  IDBObjectStore* store = StoreOf(info);
  IDBTransaction* transaction = store->transaction.Get();
  if (!CheckActive(isolate, transaction, true)) return;
  KeyRange range;
  if (!ToKeyRange(isolate, Arg(info, 0), true, &range)) return;
  DeleteRange(isolate, transaction, store->data, range);
  IDBRequest* request = NewRequest(isolate, transaction, store);
  Return(info, request);
  transaction->Succeed(isolate, request, UndefinedResult);
}

CALLBACK(StoreClear) {
  Isolate* isolate = info.GetIsolate();
  IDBObjectStore* store = StoreOf(info);
  IDBTransaction* transaction = store->transaction.Get();
  if (!CheckActive(isolate, transaction, true)) return;
  DeleteRange(isolate, transaction, store->data, KeyRange());
  IDBRequest* request = NewRequest(isolate, transaction, store);
  Return(info, request);
  transaction->Succeed(isolate, request, UndefinedResult);
}

CALLBACK(StoreCreateIndex) {
  Isolate* isolate = info.GetIsolate();
  IDBObjectStore* store = StoreOf(info);
  IDBTransaction* transaction = store->transaction.Get();
  if (!RequireArguments(info, 2)) return;
  if (transaction->mode != IDBTransaction::kVersionChange) {
    ThrowDOMException(isolate, "InvalidStateError",
                      "The database is not running a version change "
                      "transaction.");
    return;
  }
  if (!CheckActive(isolate, transaction, true)) return;
  auto index = std::make_unique<IndexData>();
  if (!ToStdString(info, info[0], &index->name) ||
      !ParseKeyPath(info, info[1], &index->key_path)) {
    return;
  }
  if (index->key_path.is_null) {
    ThrowDOMException(isolate, "SyntaxError",
                      "The keyPath argument contains an invalid key path.");
    return;
  }
  if (Arg(info, 2)->IsObject()) {
    Local<Object> options = info[2].As<Object>();
    Local<Value> unique, multi_entry;
    if (!GetProperty(isolate, options, "unique", &unique) ||
        !GetProperty(isolate, options, "multiEntry", &multi_entry)) {
      return;
    }
    index->unique = unique->BooleanValue(isolate);
    index->multi_entry = multi_entry->BooleanValue(isolate);
  }
  StoreData* data = store->data.get();
  if (data->FindIndex(index->name)) {
    ThrowDOMException(isolate, "ConstraintError",
                      "An index with the specified name already exists.");
    return;
  }
  // Index the records the store already has.
  for (auto& [key, record] : data->records) {
    record.index_keys.resize(data->indexes.size());
    std::vector<IDBKey> keys =
        IndexKeysFor(isolate, Deserialize(isolate, record.value), *index);
    for (const IDBKey& index_key : keys) {
      index->entries.insert({index_key, key});
    }
    record.index_keys.push_back(std::move(keys));
  }
  std::string name = index->name;
  data->indexes.push_back(std::move(index));
  Return(info, MakeGarbageCollected<IDBIndex>(isolate, store, name));
}

CALLBACK(StoreDeleteIndex) {
  Isolate* isolate = info.GetIsolate();
  IDBObjectStore* store = StoreOf(info);
  std::string name;
  if (!RequireArguments(info, 1) || !ToStdString(info, info[0], &name)) return;
  if (store->transaction->mode != IDBTransaction::kVersionChange) {
    ThrowDOMException(isolate, "InvalidStateError",
                      "The database is not running a version change "
                      "transaction.");
    return;
  }
  StoreData* data = store->data.get();
  int position = data->IndexPosition(data->FindIndex(name));
  if (position < 0) {
    ThrowDOMException(isolate, "NotFoundError",
                      "The specified index was not found.");
    return;
  }
  for (auto& [key, record] : data->records) {
    if (static_cast<size_t>(position) < record.index_keys.size()) {
      record.index_keys.erase(record.index_keys.begin() + position);
    }
  }
  data->indexes.erase(data->indexes.begin() + position);
}

void InstallObjectStoreBindings(Isolate* isolate) {
  const Interface kStore = Interface::kIDBObjectStore;
  DefineAccessor(isolate, kStore, "name", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        ToV8StringFromUtf8(info.GetIsolate(), StoreOf(info)->data->name));
  });
  DefineAccessor(isolate, kStore, "keyPath", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        KeyPathToValue(info.GetIsolate(), StoreOf(info)->data->key_path));
  });
  DefineAccessor(
      isolate, kStore, "autoIncrement", [](const CallbackInfo& info) {
        info.GetReturnValue().Set(StoreOf(info)->data->auto_increment);
      });
  DefineAccessor(isolate, kStore, "transaction", [](const CallbackInfo& info) {
    Return(info, StoreOf(info)->transaction.Get());
  });
  DefineAccessor(isolate, kStore, "indexNames", [](const CallbackInfo& info) {
    ReturnNames(info, StoreOf(info)->data->indexes);
  });
  DefineMethod(
      isolate, kStore, "put",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        PutRecord(info, StoreOf(info), StoreOf(info), info[0], Arg(info, 1),
                  false, nullptr);
      },
      1);
  DefineMethod(
      isolate, kStore, "add",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        PutRecord(info, StoreOf(info), StoreOf(info), info[0], Arg(info, 1),
                  true, nullptr);
      },
      1);
  DefineMethod(isolate, kStore, "delete", StoreDelete, 1);
  DefineMethod(isolate, kStore, "clear", StoreClear);
  DefineMethod(
      isolate, kStore, "get",
      [](const CallbackInfo& info) {
        ReadRecords(info, StoreOf(info), nullptr, ReadKind::kValue, false);
      },
      1);
  DefineMethod(
      isolate, kStore, "getKey",
      [](const CallbackInfo& info) {
        ReadRecords(info, StoreOf(info), nullptr, ReadKind::kPrimaryKey, false);
      },
      1);
  DefineMethod(isolate, kStore, "getAll", [](const CallbackInfo& info) {
    ReadRecords(info, StoreOf(info), nullptr, ReadKind::kValue, true);
  });
  DefineMethod(isolate, kStore, "getAllKeys", [](const CallbackInfo& info) {
    ReadRecords(info, StoreOf(info), nullptr, ReadKind::kPrimaryKey, true);
  });
  DefineMethod(isolate, kStore, "count", [](const CallbackInfo& info) {
    CountRecords(info, StoreOf(info), nullptr);
  });
  DefineMethod(isolate, kStore, "openCursor", [](const CallbackInfo& info) {
    OpenCursor(info, StoreOf(info), nullptr, false);
  });
  DefineMethod(isolate, kStore, "openKeyCursor", [](const CallbackInfo& info) {
    OpenCursor(info, StoreOf(info), nullptr, true);
  });
  DefineMethod(
      isolate, kStore, "index",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        IDBObjectStore* store = StoreOf(info);
        std::string name;
        if (!RequireArguments(info, 1) || !ToStdString(info, info[0], &name)) {
          return;
        }
        if (!store->data->FindIndex(name)) {
          ThrowDOMException(isolate, "NotFoundError",
                            "The specified index was not found.");
          return;
        }
        Return(info, MakeGarbageCollected<IDBIndex>(isolate, store, name));
      },
      1);
  DefineMethod(isolate, kStore, "createIndex", StoreCreateIndex, 2);
  DefineMethod(isolate, kStore, "deleteIndex", StoreDeleteIndex, 1);
}

IDBIndex* IndexOf(const CallbackInfo& info) { return Receiver<IDBIndex>(info); }

// Throws and returns null if the index has been deleted.
IndexData* IndexDataOf(const CallbackInfo& info) {
  IndexData* data = IndexOf(info)->data();
  if (!data) {
    ThrowDOMException(info.GetIsolate(), "InvalidStateError",
                      "The index has been deleted.");
  }
  return data;
}

void InstallIndexBindings(Isolate* isolate) {
  const Interface kIndex = Interface::kIDBIndex;
  DefineAccessor(isolate, kIndex, "name", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(
        ToV8StringFromUtf8(info.GetIsolate(), IndexOf(info)->name));
  });
  DefineAccessor(isolate, kIndex, "objectStore", [](const CallbackInfo& info) {
    Return(info, IndexOf(info)->store.Get());
  });
  DefineAccessor(isolate, kIndex, "keyPath", [](const CallbackInfo& info) {
    if (IndexData* data = IndexDataOf(info)) {
      info.GetReturnValue().Set(
          KeyPathToValue(info.GetIsolate(), data->key_path));
    }
  });
  DefineAccessor(isolate, kIndex, "unique", [](const CallbackInfo& info) {
    if (IndexData* data = IndexDataOf(info)) {
      info.GetReturnValue().Set(data->unique);
    }
  });
  DefineAccessor(isolate, kIndex, "multiEntry", [](const CallbackInfo& info) {
    if (IndexData* data = IndexDataOf(info)) {
      info.GetReturnValue().Set(data->multi_entry);
    }
  });
  DefineMethod(
      isolate, kIndex, "get",
      [](const CallbackInfo& info) {
        ReadRecords(info, IndexOf(info)->store.Get(), IndexOf(info),
                    ReadKind::kValue, false);
      },
      1);
  DefineMethod(
      isolate, kIndex, "getKey",
      [](const CallbackInfo& info) {
        ReadRecords(info, IndexOf(info)->store.Get(), IndexOf(info),
                    ReadKind::kPrimaryKey, false);
      },
      1);
  DefineMethod(isolate, kIndex, "getAll", [](const CallbackInfo& info) {
    ReadRecords(info, IndexOf(info)->store.Get(), IndexOf(info),
                ReadKind::kValue, true);
  });
  DefineMethod(isolate, kIndex, "getAllKeys", [](const CallbackInfo& info) {
    ReadRecords(info, IndexOf(info)->store.Get(), IndexOf(info),
                ReadKind::kPrimaryKey, true);
  });
  DefineMethod(isolate, kIndex, "count", [](const CallbackInfo& info) {
    if (IndexDataOf(info)) {
      CountRecords(info, IndexOf(info)->store.Get(), IndexOf(info));
    }
  });
  DefineMethod(isolate, kIndex, "openCursor", [](const CallbackInfo& info) {
    if (IndexDataOf(info)) {
      OpenCursor(info, IndexOf(info)->store.Get(), IndexOf(info), false);
    }
  });
  DefineMethod(isolate, kIndex, "openKeyCursor", [](const CallbackInfo& info) {
    if (IndexDataOf(info)) {
      OpenCursor(info, IndexOf(info)->store.Get(), IndexOf(info), true);
    }
  });
}

IDBCursor* CursorOf(const CallbackInfo& info) {
  return Receiver<IDBCursor>(info);
}

// continue(key) and advance(count).
void MoveCursor(const CallbackInfo& info, const IDBKey& target, int steps) {
  Isolate* isolate = info.GetIsolate();
  IDBCursor* cursor = CursorOf(info);
  if (!CheckCursor(isolate, cursor, false)) return;
  if (target.valid()) {
    int c = CompareKeys(target, cursor->key);
    if (cursor->forward() ? c <= 0 : c >= 0) {
      ThrowDOMException(isolate, "DataError",
                        "The parameter is not after the cursor's position.");
      return;
    }
  }
  for (int i = 0; i < steps; ++i) {
    if (!cursor->Step(target)) break;
  }
  DeliverCursor(isolate, cursor);
}

void InstallCursorBindings(Isolate* isolate) {
  const Interface kCursor = Interface::kIDBCursor;
  DefineAccessor(isolate, kCursor, "source", [](const CallbackInfo& info) {
    IDBCursor* cursor = CursorOf(info);
    if (cursor->index) {
      Return(info, cursor->index.Get());
    } else {
      Return(info, cursor->store.Get());
    }
  });
  DefineAccessor(isolate, kCursor, "direction", [](const CallbackInfo& info) {
    static const char* const kNames[] = {"next", "nextunique", "prev",
                                         "prevunique"};
    info.GetReturnValue().Set(
        V8Name(info.GetIsolate(), kNames[CursorOf(info)->direction]));
  });
  DefineAccessor(isolate, kCursor, "key", [](const CallbackInfo& info) {
    IDBCursor* cursor = CursorOf(info);
    if (!cursor->key_value.IsEmpty()) {
      info.GetReturnValue().Set(cursor->key_value.Get(info.GetIsolate()));
    }
  });
  DefineAccessor(isolate, kCursor, "primaryKey", [](const CallbackInfo& info) {
    IDBCursor* cursor = CursorOf(info);
    if (!cursor->primary_key_value.IsEmpty()) {
      info.GetReturnValue().Set(
          cursor->primary_key_value.Get(info.GetIsolate()));
    }
  });
  DefineAccessor(isolate, kCursor, "request", [](const CallbackInfo& info) {
    Return(info, CursorOf(info)->request.Get());
  });
  DefineAccessor(
      isolate, Interface::kIDBCursorWithValue, "value",
      [](const CallbackInfo& info) {
        IDBCursor* cursor = CursorOf(info);
        if (!cursor->value.IsEmpty()) {
          info.GetReturnValue().Set(cursor->value.Get(info.GetIsolate()));
        }
      });
  DefineMethod(isolate, kCursor, "continue", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    IDBKey target;
    if (!Arg(info, 0)->IsUndefined()) {
      target = KeyFromValue(isolate, info[0]);
      if (!target.valid()) {
        ThrowDOMException(isolate, "DataError",
                          "The parameter is not a valid key.");
        return;
      }
    }
    MoveCursor(info, target, 1);
  });
  DefineMethod(
      isolate, kCursor, "advance",
      [](const CallbackInfo& info) {
        double count = 0;
        if (!RequireArguments(info, 1) || !ToNumber(info, info[0], &count)) {
          return;
        }
        if (!(count >= 1)) {
          ThrowTypeError(info.GetIsolate(),
                         "A count argument with value 0 (zero) was supplied, "
                         "must be greater than 0.");
          return;
        }
        MoveCursor(info, IDBKey(), static_cast<int>(count));
      },
      1);
  DefineMethod(
      isolate, kCursor, "update",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        IDBCursor* cursor = CursorOf(info);
        if (!RequireArguments(info, 1) || !CheckCursor(isolate, cursor, true)) {
          return;
        }
        if (cursor->key_only()) {
          ThrowDOMException(isolate, "InvalidStateError",
                            "The cursor is a key cursor.");
          return;
        }
        IDBKey key = cursor->primary_key;
        PutRecord(info, cursor->store.Get(), cursor, info[0],
                  Undefined(isolate), false, &key);
      },
      1);
  DefineMethod(isolate, kCursor, "delete", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    IDBCursor* cursor = CursorOf(info);
    if (!CheckCursor(isolate, cursor, true)) return;
    IDBTransaction* transaction = cursor->store->transaction.Get();
    KeyRange range;
    range.lower = range.upper = cursor->primary_key;
    DeleteRange(isolate, transaction, cursor->store->data, range);
    IDBRequest* request = NewRequest(isolate, transaction, cursor);
    Return(info, request);
    transaction->Succeed(isolate, request, UndefinedResult);
  });
}

// --- IDBFactory --------------------------------------------------------------

class IDBFactory final : public ScriptWrappable {
 public:
  explicit IDBFactory(Window* window)
      : ScriptWrappable(Interface::kIDBFactory), window(window) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(window);
  }
  Window* WrapperWindow() const override { return window.Get(); }
  Member<Window> window;
};

// Runs the steps of open() once the request's task comes up.
void OpenDatabase(Isolate* isolate, IDBRequest* request,
                  const std::string& name, double requested_version) {
  Window* window = request->window.Get();
  std::shared_ptr<DatabaseData>& slot = Databases()[name];
  if (!slot) {
    slot = std::make_shared<DatabaseData>();
    slot->name = name;
  }
  std::shared_ptr<DatabaseData> data = slot;
  const double old_version = data->version;
  if (requested_version == 0) {
    requested_version = old_version == 0 ? 1 : old_version;
  }
  if (requested_version < old_version) {
    request->error.Reset(
        isolate, NewDOMException(isolate, "VersionError",
                                 "The requested version is less than the "
                                 "existing version."));
    request->done = true;
    FireEvent(isolate, request, NewEvent(isolate, "error", true, true));
    return;
  }
  IDBDatabase* db = MakeGarbageCollected<IDBDatabase>(isolate, window, data);
  request->result.Reset(isolate, ToV8(isolate, db));
  request->done = true;
  if (requested_version == old_version) {
    FireEvent(isolate, request, NewEvent(isolate, "success"));
    return;
  }
  data->version = requested_version;
  std::vector<std::string> scope;
  for (const auto& store : data->stores) scope.push_back(store->name);
  IDBTransaction* transaction = MakeGarbageCollected<IDBTransaction>(
      isolate, window, db, IDBTransaction::kVersionChange, std::move(scope));
  transaction->open_request = request;
  db->upgrade_transaction = transaction;
  request->transaction = transaction;
  FireEvent(isolate, request,
            NewVersionChangeEvent(isolate, "upgradeneeded", old_version,
                                  Number::New(isolate, requested_version)));
  transaction->active = false;
  // Fires `complete` and then the request's `success`.
  transaction->MaybeCommit(isolate);
}

IDBRequest* NewOpenRequest(const CallbackInfo& info) {
  Isolate* isolate = info.GetIsolate();
  IDBRequest* request = MakeGarbageCollected<IDBRequest>(
      isolate, Interface::kIDBOpenDBRequest,
      Receiver<IDBFactory>(info)->window.Get());
  Return(info, request);
  return request;
}

void InstallFactoryBindings(Isolate* isolate) {
  const Interface kFactory = Interface::kIDBFactory;
  DefineMethod(
      isolate, kFactory, "open",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        std::string name;
        double version = 0;
        if (!RequireArguments(info, 1) || !ToStdString(info, info[0], &name)) {
          return;
        }
        if (!Arg(info, 1)->IsUndefined()) {
          if (!ToNumber(info, info[1], &version)) return;
          if (!(version >= 1)) {
            ThrowTypeError(isolate, "The version provided must not be 0.");
            return;
          }
          version = std::floor(version);
        }
        cppgc::Persistent<IDBRequest> request(NewOpenRequest(info));
        PostBackendTask(
            isolate, kFactoryLatencyMs, [isolate, request, name, version] {
              if (request->window->detached) return;
              HandleScope handle_scope(isolate);
              Context::Scope context_scope(request->window->context(isolate));
              OpenDatabase(isolate, request.Get(), name, version);
            });
      },
      1);
  DefineMethod(
      isolate, kFactory, "deleteDatabase",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        std::string name;
        if (!RequireArguments(info, 1) || !ToStdString(info, info[0], &name)) {
          return;
        }
        cppgc::Persistent<IDBRequest> request(NewOpenRequest(info));
        PostBackendTask(isolate, kFactoryLatencyMs, [isolate, request, name] {
          if (request->window->detached) return;
          HandleScope handle_scope(isolate);
          Context::Scope context_scope(request->window->context(isolate));
          auto it = Databases().find(name);
          double old_version = 0;
          if (it != Databases().end()) {
            old_version = it->second->version;
            Databases().erase(it);
          }
          request->done = true;
          FireEvent(isolate, request.Get(),
                    NewVersionChangeEvent(isolate, "success", old_version,
                                          Null(isolate)));
        });
      },
      1);
  DefineMethod(
      isolate, kFactory, "cmp",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 2)) return;
        IDBKey first = KeyFromValue(isolate, info[0]);
        IDBKey second = KeyFromValue(isolate, info[1]);
        if (!first.valid() || !second.valid()) {
          ThrowDOMException(isolate, "DataError",
                            "The parameter is not a valid key.");
          return;
        }
        info.GetReturnValue().Set(CompareKeys(first, second));
      },
      2);
  DefineMethod(isolate, kFactory, "databases", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    Local<Promise::Resolver> resolver;
    if (!Promise::Resolver::New(context).ToLocal(&resolver)) return;
    LocalVector<Value> entries(isolate);
    for (const auto& [name, data] : Databases()) {
      Local<Object> entry = Object::New(isolate);
      USE(entry->Set(context, V8Name(isolate, "name"),
                     ToV8StringFromUtf8(isolate, name)));
      USE(entry->Set(context, V8Name(isolate, "version"),
                     Number::New(isolate, data->version)));
      entries.push_back(entry);
    }
    USE(resolver->Resolve(context,
                          Array::New(isolate, entries.data(), entries.size())));
    info.GetReturnValue().Set(resolver->GetPromise());
  });
}

#undef CALLBACK

}  // namespace

void InstallIndexedDBBindings(Isolate* isolate) {
  InstallStringListBindings(isolate);
  InstallKeyRangeBindings(isolate);
  InstallRequestBindings(isolate);
  InstallDatabaseBindings(isolate);
  InstallTransactionBindings(isolate);
  InstallObjectStoreBindings(isolate);
  InstallIndexBindings(isolate);
  InstallCursorBindings(isolate);
  InstallFactoryBindings(isolate);
  DefineGlobalAccessor(isolate, "indexedDB", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Window* window = Window::Current(isolate);
    if (!window->indexed_db) {
      window->indexed_db = MakeGarbageCollected<IDBFactory>(isolate, window);
    }
    Return(info, window->indexed_db.Get());
  });
}

}  // namespace mock_dom
}  // namespace v8
