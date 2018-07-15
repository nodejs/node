#ifndef SRC_MEMORY_TRACKER_INL_H_
#define SRC_MEMORY_TRACKER_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "memory_tracker.h"

namespace node {

class MemoryRetainerNode : public v8::EmbedderGraph::Node {
 public:
  explicit inline MemoryRetainerNode(MemoryTracker* tracker,
                                     const MemoryRetainer* retainer,
                                     const char* name)
     : retainer_(retainer) {
    if (retainer_ != nullptr) {
      v8::HandleScope handle_scope(tracker->isolate());
      v8::Local<v8::Object> obj = retainer_->WrappedObject();
      if (!obj.IsEmpty())
        wrapper_node_ = tracker->graph()->V8Node(obj);

      name_ = retainer_->MemoryInfoName();
    }
    if (name_.empty() && name != nullptr) {
      name_ = name;
    }
  }

  const char* Name() override { return name_.c_str(); }
  const char* NamePrefix() override { return "Node /"; }
  size_t SizeInBytes() override { return size_; }
  // TODO(addaleax): Merging this with the "official" WrapperNode() method
  // seems to lose accuracy, e.g. SizeInBytes() is disregarded.
  // Figure out whether to do anything about that.
  Node* JSWrapperNode() { return wrapper_node_; }

  bool IsRootNode() override {
    return retainer_ != nullptr && retainer_->IsRootNode();
  }

 private:
  friend class MemoryTracker;

  Node* wrapper_node_ = nullptr;
  const MemoryRetainer* retainer_;
  std::string name_;
  size_t size_ = 0;
};

template <typename T>
void MemoryTracker::TrackThis(const T* obj) {
  CurrentNode()->size_ = sizeof(T);
}

void MemoryTracker::TrackFieldWithSize(const char* name, size_t size) {
  if (size > 0)
    AddNode(name)->size_ = size;
}

void MemoryTracker::TrackField(const char* name, const MemoryRetainer& value) {
  TrackField(name, &value);
}

void MemoryTracker::TrackField(const char* name, const MemoryRetainer* value) {
  if (track_only_self_ || value == nullptr) return;
  auto it = seen_.find(value);
  if (it != seen_.end()) {
    graph_->AddEdge(CurrentNode(), it->second);
  } else {
    Track(value, name);
  }
}

template <typename T>
void MemoryTracker::TrackField(const char* name,
                               const std::unique_ptr<T>& value) {
  TrackField(name, value.get());
}

template <typename T, typename Iterator>
void MemoryTracker::TrackField(const char* name, const T& value) {
  if (value.begin() == value.end()) return;
  size_t index = 0;
  PushNode(name);
  for (Iterator it = value.begin(); it != value.end(); ++it)
    TrackField(std::to_string(index++).c_str(), *it);
  PopNode();
}

template <typename T>
void MemoryTracker::TrackField(const char* name, const std::queue<T>& value) {
  struct ContainerGetter : public std::queue<T> {
    static const typename std::queue<T>::container_type& Get(
        const std::queue<T>& value) {
      return value.*&ContainerGetter::c;
    }
  };

  const auto& container = ContainerGetter::Get(value);
  TrackField(name, container);
}

template <typename T, typename test_for_number, typename dummy>
void MemoryTracker::TrackField(const char* name, const T& value) {
  // For numbers, creating new nodes is not worth the overhead.
  CurrentNode()->size_ += sizeof(T);
}

template <typename T, typename U>
void MemoryTracker::TrackField(const char* name, const std::pair<T, U>& value) {
  PushNode(name);
  TrackField("first", value.first);
  TrackField("second", value.second);
  PopNode();
}

template <typename T>
void MemoryTracker::TrackField(const char* name,
                               const std::basic_string<T>& value) {
  TrackFieldWithSize(name, value.size() * sizeof(T));
}

template <typename T, typename Traits>
void MemoryTracker::TrackField(const char* name,
                               const v8::Persistent<T, Traits>& value) {
  TrackField(name, value.Get(isolate_));
}

template <typename T>
void MemoryTracker::TrackField(const char* name, const v8::Local<T>& value) {
  if (!value.IsEmpty())
    graph_->AddEdge(CurrentNode(), graph_->V8Node(value));
}

template <typename T>
void MemoryTracker::TrackField(const char* name,
                               const MallocedBuffer<T>& value) {
  TrackFieldWithSize(name, value.size);
}

void MemoryTracker::TrackField(const char* name, const uv_buf_t& value) {
  TrackFieldWithSize(name, value.len);
}

template <class NativeT, class V8T>
void MemoryTracker::TrackField(const char* name,
                       const AliasedBuffer<NativeT, V8T>& value) {
  TrackField(name, value.GetJSArray());
}

void MemoryTracker::Track(const MemoryRetainer* value, const char* name) {
  v8::HandleScope handle_scope(isolate_);
  MemoryRetainerNode* n = PushNode(name, value);
  value->MemoryInfo(this);
  CHECK_EQ(CurrentNode(), n);
  CHECK_NE(n->size_, 0);
  PopNode();
}

MemoryRetainerNode* MemoryTracker::CurrentNode() const {
  if (node_stack_.empty()) return nullptr;
  return node_stack_.top();
}

MemoryRetainerNode* MemoryTracker::AddNode(
    const char* name, const MemoryRetainer* retainer) {
  MemoryRetainerNode* n = new MemoryRetainerNode(this, retainer, name);
  graph_->AddNode(std::unique_ptr<v8::EmbedderGraph::Node>(n));
  if (retainer != nullptr)
    seen_[retainer] = n;

  if (CurrentNode() != nullptr)
    graph_->AddEdge(CurrentNode(), n);

  if (n->JSWrapperNode() != nullptr) {
    graph_->AddEdge(n, n->JSWrapperNode());
    graph_->AddEdge(n->JSWrapperNode(), n);
  }

  return n;
}

MemoryRetainerNode* MemoryTracker::PushNode(
    const char* name, const MemoryRetainer* retainer) {
  MemoryRetainerNode* n = AddNode(name, retainer);
  node_stack_.push(n);
  return n;
}

void MemoryTracker::PopNode() {
  node_stack_.pop();
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_MEMORY_TRACKER_INL_H_
