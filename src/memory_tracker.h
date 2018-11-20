#ifndef SRC_MEMORY_TRACKER_H_
#define SRC_MEMORY_TRACKER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <unordered_map>
#include <queue>
#include <stack>
#include <string>
#include <limits>
#include <uv.h>
#include "aliased_buffer.h"
#include "v8-profiler.h"

namespace node {

class MemoryTracker;
class MemoryRetainerNode;

namespace crypto {
class NodeBIO;
}

class MemoryRetainer {
 public:
  virtual ~MemoryRetainer() {}

  // Subclasses should implement this to provide information for heap snapshots.
  virtual void MemoryInfo(MemoryTracker* tracker) const = 0;

  virtual v8::Local<v8::Object> WrappedObject() const {
    return v8::Local<v8::Object>();
  }

  virtual bool IsRootNode() const { return false; }

  virtual std::string MemoryInfoName() const { return std::string(); }
};

class MemoryTracker {
 public:
  template <typename T>
  inline void TrackThis(const T* obj);

  inline void TrackFieldWithSize(const char* name, size_t size);

  inline void TrackField(const char* name, const MemoryRetainer& value);
  inline void TrackField(const char* name, const MemoryRetainer* value);
  template <typename T>
  inline void TrackField(const char* name, const std::unique_ptr<T>& value);
  template <typename T, typename Iterator = typename T::const_iterator>
  inline void TrackField(const char* name, const T& value);
  template <typename T>
  inline void TrackField(const char* name, const std::queue<T>& value);
  template <typename T>
  inline void TrackField(const char* name, const std::basic_string<T>& value);
  template <typename T, typename test_for_number =
      typename std::enable_if<
          std::numeric_limits<T>::is_specialized, bool>::type,
      typename dummy = bool>
  inline void TrackField(const char* name, const T& value);
  template <typename T, typename U>
  inline void TrackField(const char* name, const std::pair<T, U>& value);
  template <typename T, typename Traits>
  inline void TrackField(const char* name,
                         const v8::Persistent<T, Traits>& value);
  template <typename T>
  inline void TrackField(const char* name, const v8::Local<T>& value);
  template <typename T>
  inline void TrackField(const char* name, const MallocedBuffer<T>& value);
  inline void TrackField(const char* name, const uv_buf_t& value);
  template <class NativeT, class V8T>
  inline void TrackField(const char* name,
                         const AliasedBuffer<NativeT, V8T>& value);

  inline void Track(const MemoryRetainer* value, const char* name = nullptr);

  inline void set_track_only_self(bool value) { track_only_self_ = value; }
  inline v8::EmbedderGraph* graph() { return graph_; }
  inline v8::Isolate* isolate() { return isolate_; }

  inline explicit MemoryTracker(v8::Isolate* isolate,
                                v8::EmbedderGraph* graph)
    : isolate_(isolate), graph_(graph) {}

 private:
  typedef std::unordered_map<const MemoryRetainer*, MemoryRetainerNode*>
      NodeMap;

  inline MemoryRetainerNode* CurrentNode() const;
  inline MemoryRetainerNode* AddNode(const char* name,
                                     const MemoryRetainer* retainer = nullptr);
  inline MemoryRetainerNode* PushNode(const char* name,
                                      const MemoryRetainer* retainer = nullptr);
  inline void PopNode();

  bool track_only_self_ = false;
  v8::Isolate* isolate_;
  v8::EmbedderGraph* graph_;
  std::stack<MemoryRetainerNode*> node_stack_;
  NodeMap seen_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_MEMORY_TRACKER_H_
