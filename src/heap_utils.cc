#include "diagnosticfilename-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "stream_base-inl.h"
#include "util-inl.h"

using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::EmbedderGraph;
using v8::EscapableHandleScope;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::HeapSnapshot;
using v8::Isolate;
using v8::JSON;
using v8::Local;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Value;

namespace node {
namespace heap {

class JSGraphJSNode : public EmbedderGraph::Node {
 public:
  const char* Name() override { return "<JS Node>"; }
  size_t SizeInBytes() override { return 0; }
  bool IsEmbedderNode() override { return false; }
  Local<Value> JSValue() { return PersistentToLocal::Strong(persistent_); }

  int IdentityHash() {
    Local<Value> v = JSValue();
    if (v->IsObject()) return v.As<Object>()->GetIdentityHash();
    if (v->IsName()) return v.As<v8::Name>()->GetIdentityHash();
    if (v->IsInt32()) return v.As<v8::Int32>()->Value();
    return 0;
  }

  JSGraphJSNode(Isolate* isolate, Local<Value> val)
      : persistent_(isolate, val) {
    CHECK(!val.IsEmpty());
  }

  struct Hash {
    inline size_t operator()(JSGraphJSNode* n) const {
      return static_cast<size_t>(n->IdentityHash());
    }
  };

  struct Equal {
    inline bool operator()(JSGraphJSNode* a, JSGraphJSNode* b) const {
      return a->JSValue()->SameValue(b->JSValue());
    }
  };

 private:
  Global<Value> persistent_;
};

class JSGraph : public EmbedderGraph {
 public:
  explicit JSGraph(Isolate* isolate) : isolate_(isolate) {}

  Node* V8Node(const Local<Value>& value) override {
    std::unique_ptr<JSGraphJSNode> n { new JSGraphJSNode(isolate_, value) };
    auto it = engine_nodes_.find(n.get());
    if (it != engine_nodes_.end())
      return *it;
    engine_nodes_.insert(n.get());
    return AddNode(std::unique_ptr<Node>(n.release()));
  }

  Node* AddNode(std::unique_ptr<Node> node) override {
    Node* n = node.get();
    nodes_.emplace(std::move(node));
    return n;
  }

  void AddEdge(Node* from, Node* to, const char* name = nullptr) override {
    edges_[from].insert(std::make_pair(name, to));
  }

  MaybeLocal<Array> CreateObject() const {
    EscapableHandleScope handle_scope(isolate_);
    Local<Context> context = isolate_->GetCurrentContext();
    Environment* env = Environment::GetCurrent(context);

    std::unordered_map<Node*, Local<Object>> info_objects;
    Local<Array> nodes = Array::New(isolate_, nodes_.size());
    Local<String> edges_string = FIXED_ONE_BYTE_STRING(isolate_, "edges");
    Local<String> is_root_string = FIXED_ONE_BYTE_STRING(isolate_, "isRoot");
    Local<String> name_string = env->name_string();
    Local<String> size_string = env->size_string();
    Local<String> value_string = env->value_string();
    Local<String> wraps_string = FIXED_ONE_BYTE_STRING(isolate_, "wraps");
    Local<String> to_string = FIXED_ONE_BYTE_STRING(isolate_, "to");

    for (const std::unique_ptr<Node>& n : nodes_)
      info_objects[n.get()] = Object::New(isolate_);

    {
      HandleScope handle_scope(isolate_);
      size_t i = 0;
      for (const std::unique_ptr<Node>& n : nodes_) {
        Local<Object> obj = info_objects[n.get()];
        Local<Value> value;
        std::string name_str;
        const char* prefix = n->NamePrefix();
        if (prefix == nullptr) {
          name_str = n->Name();
        } else {
          name_str = n->NamePrefix();
          name_str += " ";
          name_str += n->Name();
        }
        if (!String::NewFromUtf8(
                 isolate_, name_str.c_str(), v8::NewStringType::kNormal)
                 .ToLocal(&value) ||
            obj->Set(context, name_string, value).IsNothing() ||
            obj->Set(context,
                     is_root_string,
                     Boolean::New(isolate_, n->IsRootNode()))
                .IsNothing() ||
            obj->Set(context,
                     size_string,
                     Number::New(isolate_, n->SizeInBytes()))
                .IsNothing() ||
            obj->Set(context, edges_string, Array::New(isolate_)).IsNothing()) {
          return MaybeLocal<Array>();
        }
        if (nodes->Set(context, i++, obj).IsNothing())
          return MaybeLocal<Array>();
        if (!n->IsEmbedderNode()) {
          value = static_cast<JSGraphJSNode*>(n.get())->JSValue();
          if (obj->Set(context, value_string, value).IsNothing())
            return MaybeLocal<Array>();
        }
      }
    }

    for (const std::unique_ptr<Node>& n : nodes_) {
      Node* wraps = n->WrapperNode();
      if (wraps == nullptr) continue;
      Local<Object> from = info_objects[n.get()];
      Local<Object> to = info_objects[wraps];
      if (from->Set(context, wraps_string, to).IsNothing())
        return MaybeLocal<Array>();
    }

    for (const auto& edge_info : edges_) {
      Node* source = edge_info.first;
      Local<Value> edges;
      if (!info_objects[source]->Get(context, edges_string).ToLocal(&edges) ||
          !edges->IsArray()) {
        return MaybeLocal<Array>();
      }

      size_t i = 0;
      size_t j = 0;
      for (const auto& edge : edge_info.second) {
        Local<Object> to_object = info_objects[edge.second];
        Local<Object> edge_obj = Object::New(isolate_);
        Local<Value> edge_name_value;
        const char* edge_name = edge.first;
        if (edge_name != nullptr) {
          if (!String::NewFromUtf8(
                  isolate_, edge_name, v8::NewStringType::kNormal)
                  .ToLocal(&edge_name_value)) {
            return MaybeLocal<Array>();
          }
        } else {
          edge_name_value = Number::New(isolate_, j++);
        }
        if (edge_obj->Set(context, name_string, edge_name_value).IsNothing() ||
            edge_obj->Set(context, to_string, to_object).IsNothing() ||
            edges.As<Array>()->Set(context, i++, edge_obj).IsNothing()) {
          return MaybeLocal<Array>();
        }
      }
    }

    return handle_scope.Escape(nodes);
  }

 private:
  Isolate* isolate_;
  std::unordered_set<std::unique_ptr<Node>> nodes_;
  std::unordered_set<JSGraphJSNode*, JSGraphJSNode::Hash, JSGraphJSNode::Equal>
      engine_nodes_;
  std::unordered_map<Node*, std::set<std::pair<const char*, Node*>>> edges_;
};

void BuildEmbedderGraph(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  JSGraph graph(env->isolate());
  Environment::BuildEmbedderGraph(env->isolate(), &graph, env);
  Local<Array> ret;
  if (graph.CreateObject().ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

namespace {
class FileOutputStream : public v8::OutputStream {
 public:
  explicit FileOutputStream(FILE* stream) : stream_(stream) {}

  int GetChunkSize() override {
    return 65536;  // big chunks == faster
  }

  void EndOfStream() override {}

  WriteResult WriteAsciiChunk(char* data, int size) override {
    const size_t len = static_cast<size_t>(size);
    size_t off = 0;

    while (off < len && !feof(stream_) && !ferror(stream_))
      off += fwrite(data + off, 1, len - off, stream_);

    return off == len ? kContinue : kAbort;
  }

 private:
  FILE* stream_;
};

class HeapSnapshotStream : public AsyncWrap,
                           public StreamBase,
                           public v8::OutputStream {
 public:
  HeapSnapshotStream(
      Environment* env,
      HeapSnapshotPointer&& snapshot,
      v8::Local<v8::Object> obj) :
      AsyncWrap(env, obj, AsyncWrap::PROVIDER_HEAPSNAPSHOT),
      StreamBase(env),
      snapshot_(std::move(snapshot)) {
    MakeWeak();
    StreamBase::AttachToObject(GetObject());
  }

  ~HeapSnapshotStream() override {}

  int GetChunkSize() override {
    return 65536;  // big chunks == faster
  }

  void EndOfStream() override {
    EmitRead(UV_EOF);
    snapshot_.reset();
  }

  WriteResult WriteAsciiChunk(char* data, int size) override {
    int len = size;
    while (len != 0) {
      uv_buf_t buf = EmitAlloc(size);
      ssize_t avail = len;
      if (static_cast<ssize_t>(buf.len) < avail)
        avail = buf.len;
      memcpy(buf.base, data, avail);
      data += avail;
      len -= avail;
      EmitRead(size, buf);
    }
    return kContinue;
  }

  int ReadStart() override {
    CHECK_NE(snapshot_, nullptr);
    snapshot_->Serialize(this, HeapSnapshot::kJSON);
    return 0;
  }

  int ReadStop() override {
    return 0;
  }

  int DoShutdown(ShutdownWrap* req_wrap) override {
    UNREACHABLE();
  }

  int DoWrite(WriteWrap* w,
              uv_buf_t* bufs,
              size_t count,
              uv_stream_t* send_handle) override {
    UNREACHABLE();
  }

  bool IsAlive() override { return snapshot_ != nullptr; }
  bool IsClosing() override { return snapshot_ == nullptr; }
  AsyncWrap* GetAsyncWrap() override { return this; }

  void MemoryInfo(MemoryTracker* tracker) const override {
    if (snapshot_ != nullptr) {
      tracker->TrackFieldWithSize(
          "snapshot", sizeof(*snapshot_), "HeapSnapshot");
    }
  }

  SET_MEMORY_INFO_NAME(HeapSnapshotStream)
  SET_SELF_SIZE(HeapSnapshotStream)

 private:
  HeapSnapshotPointer snapshot_;
};

inline void TakeSnapshot(Isolate* isolate, v8::OutputStream* out) {
  HeapSnapshotPointer snapshot {
      isolate->GetHeapProfiler()->TakeHeapSnapshot() };
  snapshot->Serialize(out, HeapSnapshot::kJSON);
}

inline bool WriteSnapshot(Isolate* isolate, const char* filename) {
  FILE* fp = fopen(filename, "w");
  if (fp == nullptr)
    return false;
  FileOutputStream stream(fp);
  TakeSnapshot(isolate, &stream);
  fclose(fp);
  return true;
}

}  // namespace

void DeleteHeapSnapshot(const v8::HeapSnapshot* snapshot) {
  const_cast<HeapSnapshot*>(snapshot)->Delete();
}

BaseObjectPtr<AsyncWrap> CreateHeapSnapshotStream(
    Environment* env, HeapSnapshotPointer&& snapshot) {
  HandleScope scope(env->isolate());

  if (env->streambaseoutputstream_constructor_template().IsEmpty()) {
    // Create FunctionTemplate for HeapSnapshotStream
    Local<FunctionTemplate> os = FunctionTemplate::New(env->isolate());
    os->Inherit(AsyncWrap::GetConstructorTemplate(env));
    Local<ObjectTemplate> ost = os->InstanceTemplate();
    ost->SetInternalFieldCount(StreamBase::kInternalFieldCount);
    os->SetClassName(
        FIXED_ONE_BYTE_STRING(env->isolate(), "HeapSnapshotStream"));
    StreamBase::AddMethods(env, os);
    env->set_streambaseoutputstream_constructor_template(ost);
  }

  Local<Object> obj;
  if (!env->streambaseoutputstream_constructor_template()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return {};
  }
  return MakeBaseObject<HeapSnapshotStream>(env, std::move(snapshot), obj);
}

void CreateHeapSnapshotStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HeapSnapshotPointer snapshot {
      env->isolate()->GetHeapProfiler()->TakeHeapSnapshot() };
  CHECK(snapshot);
  BaseObjectPtr<AsyncWrap> stream =
      CreateHeapSnapshotStream(env, std::move(snapshot));
  if (stream)
    args.GetReturnValue().Set(stream->object());
}

void TriggerHeapSnapshot(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();

  Local<Value> filename_v = args[0];

  if (filename_v->IsUndefined()) {
    DiagnosticFilename name(env, "Heap", "heapsnapshot");
    if (!WriteSnapshot(isolate, *name))
      return;
    if (String::NewFromUtf8(isolate, *name, v8::NewStringType::kNormal)
            .ToLocal(&filename_v)) {
      args.GetReturnValue().Set(filename_v);
    }
    return;
  }

  BufferValue path(isolate, filename_v);
  CHECK_NOT_NULL(*path);
  if (!WriteSnapshot(isolate, *path))
    return;
  return args.GetReturnValue().Set(filename_v);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "buildEmbedderGraph", BuildEmbedderGraph);
  env->SetMethod(target, "triggerHeapSnapshot", TriggerHeapSnapshot);
  env->SetMethod(target, "createHeapSnapshotStream", CreateHeapSnapshotStream);
}

}  // namespace heap
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(heap_utils, node::heap::Initialize)
