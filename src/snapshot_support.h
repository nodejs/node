#ifndef SRC_SNAPSHOT_SUPPORT_H_
#define SRC_SNAPSHOT_SUPPORT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"
#include <map>

namespace node {

class BaseObject;
template <typename T, bool kIsWeak>
class BaseObjectPtrImpl;
class Environment;

// We use V8SnapshotIndex when referring to indices in the sense of the V8
// snapshot data, to avoid confusion with other kinds of indices.
using V8SnapshotIndex = size_t;

// This serves as the abstract base class for the snapshot reader and writer
// classes. It includes the current snapshot blob, and a list of errors that
// have occurred while reading/writing the snapshot data.
class SnapshotDataBase {
 public:
  virtual ~SnapshotDataBase() = default;

  // kEmptyIndex is used when we want to serialize an empty v8::Local<> handle.
  static constexpr V8SnapshotIndex kEmptyIndex = static_cast<size_t>(-1);

  struct Error {
    // The byte index into the snapshot where the error occurred.
    size_t index;
    // An error message. Currently, this includes the index, as well as the
    // stack of entries that are used for grouping data in the snapshot.
    std::string message;
  };

  // Add a single new error. Index and entry stack are added automatically.
  void add_error(const std::string& error);
  // Return the list of current errors.
  inline const std::vector<Error>& errors() const;

  // Return a copy of the underlying byte array.
  inline std::vector<uint8_t> storage();

  // Print a human-readable representation of all errors, and abort the process
  // if any exist.
  void PrintErrorsAndAbortIfAny();
  // Dump the full snapshot, including errors, to stderr.
  void DumpToStderr();

 protected:
  explicit inline SnapshotDataBase(std::vector<uint8_t>&& storage);
  SnapshotDataBase() = default;

  // Checks whether `length` more data is available in the byte array.
  // This mostly makes sense for reading data.
  bool HasSpace(size_t length) const;

  std::vector<uint8_t> storage_;

  struct State {
    size_t current_index = 0;
    std::vector<Error> errors;
    std::vector<std::string> entry_stack;
  };
  State state_;

  // Defined here so that they can be used by the v8::Local<> writing
  // and reading functions.
  static const uint8_t kContextIndependentObjectTag;
  static const uint8_t kObjectTag;

  // Used internally to temporarily save the current State of this object.
  class SaveStateScope {
   public:
    explicit SaveStateScope(SnapshotDataBase* snapshot_data);
    ~SaveStateScope();

   private:
    SnapshotDataBase* snapshot_data_;
    State state_;
  };
};

class SnapshotCreateData final : public SnapshotDataBase {
 public:
  // Start writing an entry. This must be matched by an EndWriteEntry() call.
  // The name is mostly used for debugging, but can also be used to figure out
  // how to deserialize the data for this entry.
  void StartWriteEntry(const char* name);
  void EndWriteEntry();

  // Write data of a given type into the snapshot. When reading, the read
  // function for the corresponding must be used.
  void WriteBool(bool value);
  void WriteInt32(int32_t value);
  void WriteInt64(int64_t value);
  void WriteUint32(uint32_t value);
  void WriteUint64(uint64_t value);
  void WriteIndex(V8SnapshotIndex value);
  void WriteString(const char* str, size_t length = static_cast<size_t>(-1));
  void WriteString(const std::string& str);

  // Write a V8 object into the snapshot. Use the ContextIndependent variant
  // for writing ObjectTemplate, FunctionTemplate and External objects.
  // For other values, their source context must be provided.
  template <typename T>
  inline void WriteContextIndependentObject(v8::Local<T> data);
  template <typename T>
  inline void WriteObject(v8::Local<v8::Context> context, v8::Local<T> data);
  template <typename T>
  inline void WriteBaseObjectPtr(
      v8::Local<v8::Context> context, BaseObjectPtrImpl<T, false> ptr);

  inline v8::SnapshotCreator* creator();
  inline v8::Isolate* isolate();

  explicit inline SnapshotCreateData(v8::SnapshotCreator* creator);

 private:
  void WriteTag(uint8_t tag);
  void WriteRawData(const uint8_t* data, size_t length);

  v8::SnapshotCreator* creator_;
};

class SnapshotReadData final : public SnapshotDataBase {
 public:
  enum EmptyHandleMode {
    kAllowEmpty,
    kRejectEmpty
  };

  // Start reading an entry. If `expected_name` is a string, it will be matched
  // against the string provided to StartWriteEntry(), and if there is a
  // mismatch, this function will fail. If no such check is desired, for example
  // because the type of the next entry is dynamic, `nullptr` can be passed.
  // The entry type as written by StartWriteEntry() is returned.
  v8::Maybe<std::string> StartReadEntry(const char* expected_name);
  v8::Maybe<bool> EndReadEntry();

  // Read data from the snapshot. If the read function doesn't match the write
  // function at this position in the snapshot, the function will fail and
  // return an empty Maybe.
  v8::Maybe<bool> ReadBool();
  v8::Maybe<int32_t> ReadInt32();
  v8::Maybe<int64_t> ReadInt64();
  v8::Maybe<uint32_t> ReadUint32();
  v8::Maybe<uint64_t> ReadUint64();
  v8::Maybe<V8SnapshotIndex> ReadIndex();
  v8::Maybe<std::string> ReadString();

  // Read V8 objects from the snapshot, matching the writing counterparts of
  // these functions. If `mode` is `kRejectEmpty`, reading empty handles is
  // regarded as a failure. Pass `kAllowEmpty` if it is expected that the
  // corresponding write call may write an empty object.
  // These functions do not use `v8::MaybeLocal` in order to be able to
  // distinguish between successfully reading an empty handle and unsuccessfully
  // reading data.
  template <typename T>
  inline v8::Maybe<v8::Local<T>> ReadContextIndependentObject(
      EmptyHandleMode mode = kRejectEmpty);
  template <typename T>
  inline v8::Maybe<v8::Local<T>> ReadObject(
      v8::Local<v8::Context> context, EmptyHandleMode mode = kRejectEmpty);
  template <typename T>
  inline v8::Maybe<BaseObjectPtrImpl<T, false>> ReadBaseObjectPtr(
      v8::Local<v8::Context> context, EmptyHandleMode mode = kRejectEmpty);

  // Ensure that the snapshot is finished at this point, and no further data
  // is included in the snapshot.
  v8::Maybe<bool> Finish();

  struct DumpLine {
    size_t index;
    size_t depth;
    std::string description;

    std::string ToString() const;
  };
  // Return a structured description of the snapshot contents.
  std::pair<std::vector<DumpLine>, std::vector<Error>> Dump();

  inline v8::Isolate* isolate();
  inline void set_isolate(v8::Isolate*);

  explicit inline SnapshotReadData(std::vector<uint8_t>&& storage);

 private:
  bool ReadTag(uint8_t tag);
  v8::Maybe<uint8_t> PeekTag();
  bool ReadRawData(uint8_t* data, size_t length);

  v8::Maybe<BaseObject*> GetBaseObjectFromV8Object(
      v8::Local<v8::Context> context, v8::Local<v8::Object> obj);

  v8::Isolate* isolate_;
};

class Snapshottable {
 public:
  virtual ~Snapshottable() = 0;

  // Subclasses are expected to override this. The default implementation only
  // adds an error to the snapshot data.
  virtual void Serialize(SnapshotCreateData* snapshot_data) const;
};

class ExternalReferences {
 public:
  // Create static instances of this class to register a list of external
  // references for usage in snapshotting. Usually, this includes all C++
  // binding functions. `id` can be any string, as long as it is unique
  // (e.g. the current file name as retrieved by __FILE__).
  template <typename... Args>
  inline ExternalReferences(const char* id, Args*... args);

  // Returns the list of all references collected so far, not yet terminated
  // by kEnd.
  static std::vector<intptr_t> get_list();

  static const intptr_t kEnd;  // The end-of-list marker used by V8, nullptr.

 private:
  void Register(const char* id, ExternalReferences* self);
  static std::map<std::string, ExternalReferences*>* map();
  std::vector<intptr_t> references_;

  void AddPointer(intptr_t ptr);
  inline void HandleArgs();
  template <typename T, typename... Args>
  inline void HandleArgs(T* ptr, Args*... args);
};

class BaseObjectDeserializer {
 public:
  typedef BaseObject* (*Callback)(Environment*, SnapshotReadData*);

  // Create static instances of this class to register a callback used for
  // deserializing specific types of BaseObjects. This instance will be used if
  // `name` matches the name passed to `StartWriteEntry()` when creating the
  // snapshot. `name` must be unique.
  BaseObjectDeserializer(const std::string& name, Callback callback);

  static BaseObject* Deserialize(
      const std::string& name,
      Environment* env,
      SnapshotReadData* snapshot_data);

 private:
  static std::map<std::string, Callback>* map();
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_SNAPSHOT_SUPPORT_H_
