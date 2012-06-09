// Copyright 2011 the V8 project authors. All rights reserved.
//
// Tests for heap profiler

#include "v8.h"

#include "cctest.h"
#include "heap-profiler.h"
#include "snapshot.h"
#include "utils-inl.h"
#include "../include/v8-profiler.h"

namespace {

class NamedEntriesDetector {
 public:
  NamedEntriesDetector()
      : has_A2(false), has_B2(false), has_C2(false) {
  }

  void CheckEntry(i::HeapEntry* entry) {
    if (strcmp(entry->name(), "A2") == 0) has_A2 = true;
    if (strcmp(entry->name(), "B2") == 0) has_B2 = true;
    if (strcmp(entry->name(), "C2") == 0) has_C2 = true;
  }

  void CheckAllReachables(i::HeapEntry* root) {
    i::List<i::HeapEntry*> list(10);
    list.Add(root);
    root->paint();
    CheckEntry(root);
    while (!list.is_empty()) {
      i::HeapEntry* entry = list.RemoveLast();
      i::Vector<i::HeapGraphEdge> children = entry->children();
      for (int i = 0; i < children.length(); ++i) {
        if (children[i].type() == i::HeapGraphEdge::kShortcut) continue;
        i::HeapEntry* child = children[i].to();
        if (!child->painted()) {
          list.Add(child);
          child->paint();
          CheckEntry(child);
        }
      }
    }
  }

  bool has_A2;
  bool has_B2;
  bool has_C2;
};

}  // namespace


static const v8::HeapGraphNode* GetGlobalObject(
    const v8::HeapSnapshot* snapshot) {
  CHECK_EQ(2, snapshot->GetRoot()->GetChildrenCount());
  const v8::HeapGraphNode* global_obj =
      snapshot->GetRoot()->GetChild(0)->GetToNode();
  CHECK_EQ(0, strncmp("Object", const_cast<i::HeapEntry*>(
      reinterpret_cast<const i::HeapEntry*>(global_obj))->name(), 6));
  return global_obj;
}


static const v8::HeapGraphNode* GetProperty(const v8::HeapGraphNode* node,
                                            v8::HeapGraphEdge::Type type,
                                            const char* name) {
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = node->GetChild(i);
    v8::String::AsciiValue prop_name(prop->GetName());
    if (prop->GetType() == type && strcmp(name, *prop_name) == 0)
      return prop->GetToNode();
  }
  return NULL;
}


static bool HasString(const v8::HeapGraphNode* node, const char* contents) {
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = node->GetChild(i);
    const v8::HeapGraphNode* node = prop->GetToNode();
    if (node->GetType() == v8::HeapGraphNode::kString) {
      v8::String::AsciiValue node_name(node->GetName());
      if (strcmp(contents, *node_name) == 0) return true;
    }
  }
  return false;
}


TEST(HeapSnapshot) {
  v8::HandleScope scope;
  LocalContext env2;

  CompileRun(
      "function A2() {}\n"
      "function B2(x) { return function() { return typeof x; }; }\n"
      "function C2(x) { this.x1 = x; this.x2 = x; this[1] = x; }\n"
      "var a2 = new A2();\n"
      "var b2_1 = new B2(a2), b2_2 = new B2(a2);\n"
      "var c2 = new C2(a2);");
  const v8::HeapSnapshot* snapshot_env2 =
      v8::HeapProfiler::TakeSnapshot(v8_str("env2"));
  i::HeapSnapshot* i_snapshot_env2 =
      const_cast<i::HeapSnapshot*>(
          reinterpret_cast<const i::HeapSnapshot*>(snapshot_env2));
  const v8::HeapGraphNode* global_env2 = GetGlobalObject(snapshot_env2);

  // Verify, that JS global object of env2 has '..2' properties.
  const v8::HeapGraphNode* a2_node =
      GetProperty(global_env2, v8::HeapGraphEdge::kShortcut, "a2");
  CHECK_NE(NULL, a2_node);
  CHECK_NE(
      NULL, GetProperty(global_env2, v8::HeapGraphEdge::kShortcut, "b2_1"));
  CHECK_NE(
      NULL, GetProperty(global_env2, v8::HeapGraphEdge::kShortcut, "b2_2"));
  CHECK_NE(NULL, GetProperty(global_env2, v8::HeapGraphEdge::kShortcut, "c2"));

  // Paint all nodes reachable from global object.
  NamedEntriesDetector det;
  i_snapshot_env2->ClearPaint();
  det.CheckAllReachables(const_cast<i::HeapEntry*>(
      reinterpret_cast<const i::HeapEntry*>(global_env2)));
  CHECK(det.has_A2);
  CHECK(det.has_B2);
  CHECK(det.has_C2);
}


TEST(HeapSnapshotObjectSizes) {
  v8::HandleScope scope;
  LocalContext env;

  //   -a-> X1 --a
  // x -b-> X2 <-|
  CompileRun(
      "function X(a, b) { this.a = a; this.b = b; }\n"
      "x = new X(new X(), new X());\n"
      "(function() { x.a.a = x.b; })();");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("sizes"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* x =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "x");
  CHECK_NE(NULL, x);
  const v8::HeapGraphNode* x1 =
      GetProperty(x, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, x1);
  const v8::HeapGraphNode* x2 =
      GetProperty(x, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, x2);

  // Test sizes.
  CHECK_EQ(x->GetSelfSize() * 3, x->GetRetainedSize());
  CHECK_EQ(x1->GetSelfSize(), x1->GetRetainedSize());
  CHECK_EQ(x2->GetSelfSize(), x2->GetRetainedSize());
}


TEST(BoundFunctionInSnapshot) {
  v8::HandleScope scope;
  LocalContext env;
  CompileRun(
      "function myFunction(a, b) { this.a = a; this.b = b; }\n"
      "function AAAAA() {}\n"
      "boundFunction = myFunction.bind(new AAAAA(), 20, new Number(12)); \n");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("sizes"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* f =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "boundFunction");
  CHECK(f);
  CHECK_EQ(v8::String::New("native_bind"), f->GetName());
  const v8::HeapGraphNode* bindings =
      GetProperty(f, v8::HeapGraphEdge::kInternal, "bindings");
  CHECK_NE(NULL, bindings);
  CHECK_EQ(v8::HeapGraphNode::kArray, bindings->GetType());
  CHECK_EQ(4, bindings->GetChildrenCount());

  const v8::HeapGraphNode* bound_this = GetProperty(
      f, v8::HeapGraphEdge::kShortcut, "bound_this");
  CHECK(bound_this);
  CHECK_EQ(v8::HeapGraphNode::kObject, bound_this->GetType());

  const v8::HeapGraphNode* bound_function = GetProperty(
      f, v8::HeapGraphEdge::kShortcut, "bound_function");
  CHECK(bound_function);
  CHECK_EQ(v8::HeapGraphNode::kClosure, bound_function->GetType());

  const v8::HeapGraphNode* bound_argument = GetProperty(
      f, v8::HeapGraphEdge::kShortcut, "bound_argument_1");
  CHECK(bound_argument);
  CHECK_EQ(v8::HeapGraphNode::kObject, bound_argument->GetType());
}


TEST(HeapSnapshotEntryChildren) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function A() { }\n"
      "a = new A;");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("children"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  for (int i = 0, count = global->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = global->GetChild(i);
    CHECK_EQ(global, prop->GetFromNode());
  }
  const v8::HeapGraphNode* a =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, a);
  for (int i = 0, count = a->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = a->GetChild(i);
    CHECK_EQ(a, prop->GetFromNode());
  }
}


TEST(HeapSnapshotCodeObjects) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function lazy(x) { return x - 1; }\n"
      "function compiled(x) { return x + 1; }\n"
      "var anonymous = (function() { return function() { return 0; } })();\n"
      "compiled(1)");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("code"));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* compiled =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "compiled");
  CHECK_NE(NULL, compiled);
  CHECK_EQ(v8::HeapGraphNode::kClosure, compiled->GetType());
  const v8::HeapGraphNode* lazy =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "lazy");
  CHECK_NE(NULL, lazy);
  CHECK_EQ(v8::HeapGraphNode::kClosure, lazy->GetType());
  const v8::HeapGraphNode* anonymous =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "anonymous");
  CHECK_NE(NULL, anonymous);
  CHECK_EQ(v8::HeapGraphNode::kClosure, anonymous->GetType());
  v8::String::AsciiValue anonymous_name(anonymous->GetName());
  CHECK_EQ("", *anonymous_name);

  // Find references to code.
  const v8::HeapGraphNode* compiled_code =
      GetProperty(compiled, v8::HeapGraphEdge::kInternal, "shared");
  CHECK_NE(NULL, compiled_code);
  const v8::HeapGraphNode* lazy_code =
      GetProperty(lazy, v8::HeapGraphEdge::kInternal, "shared");
  CHECK_NE(NULL, lazy_code);

  // Verify that non-compiled code doesn't contain references to "x"
  // literal, while compiled code does. The scope info is stored in FixedArray
  // objects attached to the SharedFunctionInfo.
  bool compiled_references_x = false, lazy_references_x = false;
  for (int i = 0, count = compiled_code->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = compiled_code->GetChild(i);
    const v8::HeapGraphNode* node = prop->GetToNode();
    if (node->GetType() == v8::HeapGraphNode::kArray) {
      if (HasString(node, "x")) {
        compiled_references_x = true;
        break;
      }
    }
  }
  for (int i = 0, count = lazy_code->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = lazy_code->GetChild(i);
    const v8::HeapGraphNode* node = prop->GetToNode();
    if (node->GetType() == v8::HeapGraphNode::kArray) {
      if (HasString(node, "x")) {
        lazy_references_x = true;
        break;
      }
    }
  }
  CHECK(compiled_references_x);
  CHECK(!lazy_references_x);
}


TEST(HeapSnapshotHeapNumbers) {
  v8::HandleScope scope;
  LocalContext env;
  CompileRun(
      "a = 1;    // a is Smi\n"
      "b = 2.5;  // b is HeapNumber");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("numbers"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_EQ(NULL, GetProperty(global, v8::HeapGraphEdge::kShortcut, "a"));
  const v8::HeapGraphNode* b =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "b");
  CHECK_NE(NULL, b);
  CHECK_EQ(v8::HeapGraphNode::kHeapNumber, b->GetType());
}

TEST(HeapSnapshotSlicedString) {
  v8::HandleScope scope;
  LocalContext env;
  CompileRun(
      "parent_string = \"123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789.\";"
      "child_string = parent_string.slice(100);");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("strings"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* parent_string =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "parent_string");
  CHECK_NE(NULL, parent_string);
  const v8::HeapGraphNode* child_string =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "child_string");
  CHECK_NE(NULL, child_string);
  const v8::HeapGraphNode* parent =
      GetProperty(child_string, v8::HeapGraphEdge::kInternal, "parent");
  CHECK_EQ(parent_string, parent);
}

TEST(HeapSnapshotInternalReferences) {
  v8::HandleScope scope;
  v8::Local<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();
  global_template->SetInternalFieldCount(2);
  LocalContext env(NULL, global_template);
  v8::Handle<v8::Object> global_proxy = env->Global();
  v8::Handle<v8::Object> global = global_proxy->GetPrototype().As<v8::Object>();
  CHECK_EQ(2, global->InternalFieldCount());
  v8::Local<v8::Object> obj = v8::Object::New();
  global->SetInternalField(0, v8_num(17));
  global->SetInternalField(1, obj);
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("internals"));
  const v8::HeapGraphNode* global_node = GetGlobalObject(snapshot);
  // The first reference will not present, because it's a Smi.
  CHECK_EQ(NULL, GetProperty(global_node, v8::HeapGraphEdge::kInternal, "0"));
  // The second reference is to an object.
  CHECK_NE(NULL, GetProperty(global_node, v8::HeapGraphEdge::kInternal, "1"));
}


// Trying to introduce a check helper for uint64_t causes many
// overloading ambiguities, so it seems easier just to cast
// them to a signed type.
#define CHECK_EQ_UINT64_T(a, b) \
  CHECK_EQ(static_cast<int64_t>(a), static_cast<int64_t>(b))
#define CHECK_NE_UINT64_T(a, b) \
  CHECK((a) != (b))  // NOLINT

TEST(HeapEntryIdsAndArrayShift) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function AnObject() {\n"
      "    this.first = 'first';\n"
      "    this.second = 'second';\n"
      "}\n"
      "var a = new Array();\n"
      "for (var i = 0; i < 10; ++i)\n"
      "  a.push(new AnObject());\n");
  const v8::HeapSnapshot* snapshot1 =
      v8::HeapProfiler::TakeSnapshot(v8_str("s1"));

  CompileRun(
      "for (var i = 0; i < 1; ++i)\n"
      "  a.shift();\n");

  HEAP->CollectAllGarbage(i::Heap::kNoGCFlags);

  const v8::HeapSnapshot* snapshot2 =
      v8::HeapProfiler::TakeSnapshot(v8_str("s2"));

  const v8::HeapGraphNode* global1 = GetGlobalObject(snapshot1);
  const v8::HeapGraphNode* global2 = GetGlobalObject(snapshot2);
  CHECK_NE_UINT64_T(0, global1->GetId());
  CHECK_EQ_UINT64_T(global1->GetId(), global2->GetId());

  const v8::HeapGraphNode* a1 =
      GetProperty(global1, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, a1);
  const v8::HeapGraphNode* e1 =
      GetProperty(a1, v8::HeapGraphEdge::kHidden, "1");
  CHECK_NE(NULL, e1);
  const v8::HeapGraphNode* k1 =
      GetProperty(e1, v8::HeapGraphEdge::kInternal, "elements");
  CHECK_NE(NULL, k1);
  const v8::HeapGraphNode* a2 =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, a2);
  const v8::HeapGraphNode* e2 =
      GetProperty(a2, v8::HeapGraphEdge::kHidden, "1");
  CHECK_NE(NULL, e2);
  const v8::HeapGraphNode* k2 =
      GetProperty(e2, v8::HeapGraphEdge::kInternal, "elements");
  CHECK_NE(NULL, k2);

  CHECK_EQ_UINT64_T(a1->GetId(), a2->GetId());
  CHECK_EQ_UINT64_T(e1->GetId(), e2->GetId());
  CHECK_EQ_UINT64_T(k1->GetId(), k2->GetId());
}

TEST(HeapEntryIdsAndGC) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function A() {}\n"
      "function B(x) { this.x = x; }\n"
      "var a = new A();\n"
      "var b = new B(a);");
  const v8::HeapSnapshot* snapshot1 =
      v8::HeapProfiler::TakeSnapshot(v8_str("s1"));

  HEAP->CollectAllGarbage(i::Heap::kNoGCFlags);

  const v8::HeapSnapshot* snapshot2 =
      v8::HeapProfiler::TakeSnapshot(v8_str("s2"));

  const v8::HeapGraphNode* global1 = GetGlobalObject(snapshot1);
  const v8::HeapGraphNode* global2 = GetGlobalObject(snapshot2);
  CHECK_NE_UINT64_T(0, global1->GetId());
  CHECK_EQ_UINT64_T(global1->GetId(), global2->GetId());
  const v8::HeapGraphNode* A1 =
      GetProperty(global1, v8::HeapGraphEdge::kProperty, "A");
  CHECK_NE(NULL, A1);
  const v8::HeapGraphNode* A2 =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "A");
  CHECK_NE(NULL, A2);
  CHECK_NE_UINT64_T(0, A1->GetId());
  CHECK_EQ_UINT64_T(A1->GetId(), A2->GetId());
  const v8::HeapGraphNode* B1 =
      GetProperty(global1, v8::HeapGraphEdge::kProperty, "B");
  CHECK_NE(NULL, B1);
  const v8::HeapGraphNode* B2 =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "B");
  CHECK_NE(NULL, B2);
  CHECK_NE_UINT64_T(0, B1->GetId());
  CHECK_EQ_UINT64_T(B1->GetId(), B2->GetId());
  const v8::HeapGraphNode* a1 =
      GetProperty(global1, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, a1);
  const v8::HeapGraphNode* a2 =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, a2);
  CHECK_NE_UINT64_T(0, a1->GetId());
  CHECK_EQ_UINT64_T(a1->GetId(), a2->GetId());
  const v8::HeapGraphNode* b1 =
      GetProperty(global1, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, b1);
  const v8::HeapGraphNode* b2 =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, b2);
  CHECK_NE_UINT64_T(0, b1->GetId());
  CHECK_EQ_UINT64_T(b1->GetId(), b2->GetId());
}


TEST(HeapSnapshotRootPreservedAfterSorting) {
  v8::HandleScope scope;
  LocalContext env;
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("s"));
  const v8::HeapGraphNode* root1 = snapshot->GetRoot();
  const_cast<i::HeapSnapshot*>(reinterpret_cast<const i::HeapSnapshot*>(
      snapshot))->GetSortedEntriesList();
  const v8::HeapGraphNode* root2 = snapshot->GetRoot();
  CHECK_EQ(root1, root2);
}


TEST(HeapEntryDominator) {
  // The graph looks like this:
  //
  //                   -> node1
  //                  a    |^
  //          -> node5     ba
  //         a             v|
  //   node6           -> node2
  //         b        a    |^
  //          -> node4     ba
  //                  b    v|
  //                   -> node3
  //
  // The dominator for all nodes is node6.

  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function X(a, b) { this.a = a; this.b = b; }\n"
      "node6 = new X(new X(new X()), new X(new X(),new X()));\n"
      "(function(){\n"
      "node6.a.a.b = node6.b.a;  // node1 -> node2\n"
      "node6.b.a.a = node6.a.a;  // node2 -> node1\n"
      "node6.b.a.b = node6.b.b;  // node2 -> node3\n"
      "node6.b.b.a = node6.b.a;  // node3 -> node2\n"
      "})();");

  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("dominators"));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  const v8::HeapGraphNode* node6 =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "node6");
  CHECK_NE(NULL, node6);
  const v8::HeapGraphNode* node5 =
      GetProperty(node6, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, node5);
  const v8::HeapGraphNode* node4 =
      GetProperty(node6, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, node4);
  const v8::HeapGraphNode* node3 =
      GetProperty(node4, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, node3);
  const v8::HeapGraphNode* node2 =
      GetProperty(node4, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, node2);
  const v8::HeapGraphNode* node1 =
      GetProperty(node5, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, node1);

  CHECK_EQ(node6, node1->GetDominatorNode());
  CHECK_EQ(node6, node2->GetDominatorNode());
  CHECK_EQ(node6, node3->GetDominatorNode());
  CHECK_EQ(node6, node4->GetDominatorNode());
  CHECK_EQ(node6, node5->GetDominatorNode());
}


namespace {

class TestJSONStream : public v8::OutputStream {
 public:
  TestJSONStream() : eos_signaled_(0), abort_countdown_(-1) {}
  explicit TestJSONStream(int abort_countdown)
      : eos_signaled_(0), abort_countdown_(abort_countdown) {}
  virtual ~TestJSONStream() {}
  virtual void EndOfStream() { ++eos_signaled_; }
  virtual WriteResult WriteAsciiChunk(char* buffer, int chars_written) {
    if (abort_countdown_ > 0) --abort_countdown_;
    if (abort_countdown_ == 0) return kAbort;
    CHECK_GT(chars_written, 0);
    i::Vector<char> chunk = buffer_.AddBlock(chars_written, '\0');
    memcpy(chunk.start(), buffer, chars_written);
    return kContinue;
  }
  void WriteTo(i::Vector<char> dest) { buffer_.WriteTo(dest); }
  int eos_signaled() { return eos_signaled_; }
  int size() { return buffer_.size(); }
 private:
  i::Collector<char> buffer_;
  int eos_signaled_;
  int abort_countdown_;
};

class AsciiResource: public v8::String::ExternalAsciiStringResource {
 public:
  explicit AsciiResource(i::Vector<char> string): data_(string.start()) {
    length_ = string.length();
  }
  virtual const char* data() const { return data_; }
  virtual size_t length() const { return length_; }
 private:
  const char* data_;
  size_t length_;
};

}  // namespace

TEST(HeapSnapshotJSONSerialization) {
  v8::HandleScope scope;
  LocalContext env;

#define STRING_LITERAL_FOR_TEST \
  "\"String \\n\\r\\u0008\\u0081\\u0101\\u0801\\u8001\""
  CompileRun(
      "function A(s) { this.s = s; }\n"
      "function B(x) { this.x = x; }\n"
      "var a = new A(" STRING_LITERAL_FOR_TEST ");\n"
      "var b = new B(a);");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("json"));
  TestJSONStream stream;
  snapshot->Serialize(&stream, v8::HeapSnapshot::kJSON);
  CHECK_GT(stream.size(), 0);
  CHECK_EQ(1, stream.eos_signaled());
  i::ScopedVector<char> json(stream.size());
  stream.WriteTo(json);

  // Verify that snapshot string is valid JSON.
  AsciiResource json_res(json);
  v8::Local<v8::String> json_string = v8::String::NewExternal(&json_res);
  env->Global()->Set(v8_str("json_snapshot"), json_string);
  v8::Local<v8::Value> snapshot_parse_result = CompileRun(
      "var parsed = JSON.parse(json_snapshot); true;");
  CHECK(!snapshot_parse_result.IsEmpty());

  // Verify that snapshot object has required fields.
  v8::Local<v8::Object> parsed_snapshot =
      env->Global()->Get(v8_str("parsed"))->ToObject();
  CHECK(parsed_snapshot->Has(v8_str("snapshot")));
  CHECK(parsed_snapshot->Has(v8_str("nodes")));
  CHECK(parsed_snapshot->Has(v8_str("strings")));

  // Get node and edge "member" offsets.
  v8::Local<v8::Value> meta_analysis_result = CompileRun(
      "var parsed_meta = parsed.nodes[0];\n"
      "var children_count_offset ="
      "    parsed_meta.fields.indexOf('children_count');\n"
      "var children_offset ="
      "    parsed_meta.fields.indexOf('children');\n"
      "var children_meta ="
      "    parsed_meta.types[children_offset];\n"
      "var child_fields_count = children_meta.fields.length;\n"
      "var child_type_offset ="
      "    children_meta.fields.indexOf('type');\n"
      "var child_name_offset ="
      "    children_meta.fields.indexOf('name_or_index');\n"
      "var child_to_node_offset ="
      "    children_meta.fields.indexOf('to_node');\n"
      "var property_type ="
      "    children_meta.types[child_type_offset].indexOf('property');\n"
      "var shortcut_type ="
      "    children_meta.types[child_type_offset].indexOf('shortcut');");
  CHECK(!meta_analysis_result.IsEmpty());

  // A helper function for processing encoded nodes.
  CompileRun(
      "function GetChildPosByProperty(pos, prop_name, prop_type) {\n"
      "  var nodes = parsed.nodes;\n"
      "  var strings = parsed.strings;\n"
      "  for (var i = 0,\n"
      "      count = nodes[pos + children_count_offset] * child_fields_count;\n"
      "      i < count; i += child_fields_count) {\n"
      "    var child_pos = pos + children_offset + i;\n"
      "    if (nodes[child_pos + child_type_offset] === prop_type\n"
      "       && strings[nodes[child_pos + child_name_offset]] === prop_name)\n"
      "        return nodes[child_pos + child_to_node_offset];\n"
      "  }\n"
      "  return null;\n"
      "}\n");
  // Get the string index using the path: <root> -> <global>.b.x.s
  v8::Local<v8::Value> string_obj_pos_val = CompileRun(
      "GetChildPosByProperty(\n"
      "  GetChildPosByProperty(\n"
      "    GetChildPosByProperty("
      "      parsed.nodes[1 + children_offset + child_to_node_offset],"
      "      \"b\",shortcut_type),\n"
      "    \"x\", property_type),"
      "  \"s\", property_type)");
  CHECK(!string_obj_pos_val.IsEmpty());
  int string_obj_pos =
      static_cast<int>(string_obj_pos_val->ToNumber()->Value());
  v8::Local<v8::Object> nodes_array =
      parsed_snapshot->Get(v8_str("nodes"))->ToObject();
  int string_index = static_cast<int>(
      nodes_array->Get(string_obj_pos + 1)->ToNumber()->Value());
  CHECK_GT(string_index, 0);
  v8::Local<v8::Object> strings_array =
      parsed_snapshot->Get(v8_str("strings"))->ToObject();
  v8::Local<v8::String> string = strings_array->Get(string_index)->ToString();
  v8::Local<v8::String> ref_string =
      CompileRun(STRING_LITERAL_FOR_TEST)->ToString();
#undef STRING_LITERAL_FOR_TEST
  CHECK_EQ(*v8::String::Utf8Value(ref_string),
           *v8::String::Utf8Value(string));
}


TEST(HeapSnapshotJSONSerializationAborting) {
  v8::HandleScope scope;
  LocalContext env;
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("abort"));
  TestJSONStream stream(5);
  snapshot->Serialize(&stream, v8::HeapSnapshot::kJSON);
  CHECK_GT(stream.size(), 0);
  CHECK_EQ(0, stream.eos_signaled());
}


static void CheckChildrenIds(const v8::HeapSnapshot* snapshot,
                             const v8::HeapGraphNode* node,
                             int level, int max_level) {
  if (level > max_level) return;
  CHECK_EQ(node, snapshot->GetNodeById(node->GetId()));
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = node->GetChild(i);
    const v8::HeapGraphNode* child =
        snapshot->GetNodeById(prop->GetToNode()->GetId());
    CHECK_EQ_UINT64_T(prop->GetToNode()->GetId(), child->GetId());
    CHECK_EQ(prop->GetToNode(), child);
    CheckChildrenIds(snapshot, child, level + 1, max_level);
  }
}


TEST(HeapSnapshotGetNodeById) {
  v8::HandleScope scope;
  LocalContext env;

  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("id"));
  const v8::HeapGraphNode* root = snapshot->GetRoot();
  CheckChildrenIds(snapshot, root, 0, 3);
  // Check a big id, which should not exist yet.
  CHECK_EQ(NULL, snapshot->GetNodeById(0x1000000UL));
}


namespace {

class TestActivityControl : public v8::ActivityControl {
 public:
  explicit TestActivityControl(int abort_count)
      : done_(0), total_(0), abort_count_(abort_count) {}
  ControlOption ReportProgressValue(int done, int total) {
    done_ = done;
    total_ = total;
    return --abort_count_ != 0 ? kContinue : kAbort;
  }
  int done() { return done_; }
  int total() { return total_; }

 private:
  int done_;
  int total_;
  int abort_count_;
};
}

TEST(TakeHeapSnapshotAborting) {
  v8::HandleScope scope;
  LocalContext env;

  const int snapshots_count = v8::HeapProfiler::GetSnapshotsCount();
  TestActivityControl aborting_control(1);
  const v8::HeapSnapshot* no_snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("abort"),
                                     v8::HeapSnapshot::kFull,
                                     &aborting_control);
  CHECK_EQ(NULL, no_snapshot);
  CHECK_EQ(snapshots_count, v8::HeapProfiler::GetSnapshotsCount());
  CHECK_GT(aborting_control.total(), aborting_control.done());

  TestActivityControl control(-1);  // Don't abort.
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("full"),
                                     v8::HeapSnapshot::kFull,
                                     &control);
  CHECK_NE(NULL, snapshot);
  CHECK_EQ(snapshots_count + 1, v8::HeapProfiler::GetSnapshotsCount());
  CHECK_EQ(control.total(), control.done());
  CHECK_GT(control.total(), 0);
}


namespace {

class TestRetainedObjectInfo : public v8::RetainedObjectInfo {
 public:
  TestRetainedObjectInfo(int hash,
                         const char* group_label,
                         const char* label,
                         intptr_t element_count = -1,
                         intptr_t size = -1)
      : disposed_(false),
        hash_(hash),
        group_label_(group_label),
        label_(label),
        element_count_(element_count),
        size_(size) {
    instances.Add(this);
  }
  virtual ~TestRetainedObjectInfo() {}
  virtual void Dispose() {
    CHECK(!disposed_);
    disposed_ = true;
  }
  virtual bool IsEquivalent(RetainedObjectInfo* other) {
    return GetHash() == other->GetHash();
  }
  virtual intptr_t GetHash() { return hash_; }
  virtual const char* GetGroupLabel() { return group_label_; }
  virtual const char* GetLabel() { return label_; }
  virtual intptr_t GetElementCount() { return element_count_; }
  virtual intptr_t GetSizeInBytes() { return size_; }
  bool disposed() { return disposed_; }

  static v8::RetainedObjectInfo* WrapperInfoCallback(
      uint16_t class_id, v8::Handle<v8::Value> wrapper) {
    if (class_id == 1) {
      if (wrapper->IsString()) {
        v8::String::AsciiValue ascii(wrapper);
        if (strcmp(*ascii, "AAA") == 0)
          return new TestRetainedObjectInfo(1, "aaa-group", "aaa", 100);
        else if (strcmp(*ascii, "BBB") == 0)
          return new TestRetainedObjectInfo(1, "aaa-group", "aaa", 100);
      }
    } else if (class_id == 2) {
      if (wrapper->IsString()) {
        v8::String::AsciiValue ascii(wrapper);
        if (strcmp(*ascii, "CCC") == 0)
          return new TestRetainedObjectInfo(2, "ccc-group", "ccc");
      }
    }
    CHECK(false);
    return NULL;
  }

  static i::List<TestRetainedObjectInfo*> instances;

 private:
  bool disposed_;
  int category_;
  int hash_;
  const char* group_label_;
  const char* label_;
  intptr_t element_count_;
  intptr_t size_;
};


i::List<TestRetainedObjectInfo*> TestRetainedObjectInfo::instances;
}


static const v8::HeapGraphNode* GetNode(const v8::HeapGraphNode* parent,
                                        v8::HeapGraphNode::Type type,
                                        const char* name) {
  for (int i = 0, count = parent->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphNode* node = parent->GetChild(i)->GetToNode();
    if (node->GetType() == type && strcmp(name,
               const_cast<i::HeapEntry*>(
                   reinterpret_cast<const i::HeapEntry*>(node))->name()) == 0) {
      return node;
    }
  }
  return NULL;
}


TEST(HeapSnapshotRetainedObjectInfo) {
  v8::HandleScope scope;
  LocalContext env;

  v8::HeapProfiler::DefineWrapperClass(
      1, TestRetainedObjectInfo::WrapperInfoCallback);
  v8::HeapProfiler::DefineWrapperClass(
      2, TestRetainedObjectInfo::WrapperInfoCallback);
  v8::Persistent<v8::String> p_AAA =
      v8::Persistent<v8::String>::New(v8_str("AAA"));
  p_AAA.SetWrapperClassId(1);
  v8::Persistent<v8::String> p_BBB =
      v8::Persistent<v8::String>::New(v8_str("BBB"));
  p_BBB.SetWrapperClassId(1);
  v8::Persistent<v8::String> p_CCC =
      v8::Persistent<v8::String>::New(v8_str("CCC"));
  p_CCC.SetWrapperClassId(2);
  CHECK_EQ(0, TestRetainedObjectInfo::instances.length());
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("retained"));

  CHECK_EQ(3, TestRetainedObjectInfo::instances.length());
  for (int i = 0; i < TestRetainedObjectInfo::instances.length(); ++i) {
    CHECK(TestRetainedObjectInfo::instances[i]->disposed());
    delete TestRetainedObjectInfo::instances[i];
  }

  const v8::HeapGraphNode* native_group_aaa = GetNode(
      snapshot->GetRoot(), v8::HeapGraphNode::kSynthetic, "aaa-group");
  CHECK_NE(NULL, native_group_aaa);
  CHECK_EQ(1, native_group_aaa->GetChildrenCount());
  const v8::HeapGraphNode* aaa = GetNode(
      native_group_aaa, v8::HeapGraphNode::kNative, "aaa / 100 entries");
  CHECK_NE(NULL, aaa);
  CHECK_EQ(2, aaa->GetChildrenCount());

  const v8::HeapGraphNode* native_group_ccc = GetNode(
      snapshot->GetRoot(), v8::HeapGraphNode::kSynthetic, "ccc-group");
  const v8::HeapGraphNode* ccc = GetNode(
      native_group_ccc, v8::HeapGraphNode::kNative, "ccc");
  CHECK_NE(NULL, ccc);

  const v8::HeapGraphNode* n_AAA = GetNode(
      aaa, v8::HeapGraphNode::kString, "AAA");
  CHECK_NE(NULL, n_AAA);
  const v8::HeapGraphNode* n_BBB = GetNode(
      aaa, v8::HeapGraphNode::kString, "BBB");
  CHECK_NE(NULL, n_BBB);
  CHECK_EQ(1, ccc->GetChildrenCount());
  const v8::HeapGraphNode* n_CCC = GetNode(
      ccc, v8::HeapGraphNode::kString, "CCC");
  CHECK_NE(NULL, n_CCC);

  CHECK_EQ(aaa, GetProperty(n_AAA, v8::HeapGraphEdge::kInternal, "native"));
  CHECK_EQ(aaa, GetProperty(n_BBB, v8::HeapGraphEdge::kInternal, "native"));
  CHECK_EQ(ccc, GetProperty(n_CCC, v8::HeapGraphEdge::kInternal, "native"));
}


class GraphWithImplicitRefs {
 public:
  static const int kObjectsCount = 4;
  explicit GraphWithImplicitRefs(LocalContext* env) {
    CHECK_EQ(NULL, instance_);
    instance_ = this;
    for (int i = 0; i < kObjectsCount; i++) {
      objects_[i] = v8::Persistent<v8::Object>::New(v8::Object::New());
    }
    (*env)->Global()->Set(v8_str("root_object"), objects_[0]);
  }
  ~GraphWithImplicitRefs() {
    instance_ = NULL;
  }

  static void gcPrologue() {
    instance_->AddImplicitReferences();
  }

 private:
  void AddImplicitReferences() {
    // 0 -> 1
    v8::V8::AddImplicitReferences(
        v8::Persistent<v8::Object>::Cast(objects_[0]), &objects_[1], 1);
    // Adding two more references(note length=2 in params): 1 -> 2, 1 -> 3
    v8::V8::AddImplicitReferences(
        v8::Persistent<v8::Object>::Cast(objects_[1]), &objects_[2], 2);
  }

  v8::Persistent<v8::Value> objects_[kObjectsCount];
  static GraphWithImplicitRefs* instance_;
};

GraphWithImplicitRefs* GraphWithImplicitRefs::instance_ = NULL;


TEST(HeapSnapshotImplicitReferences) {
  v8::HandleScope scope;
  LocalContext env;

  GraphWithImplicitRefs graph(&env);
  v8::V8::SetGlobalGCPrologueCallback(&GraphWithImplicitRefs::gcPrologue);

  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("implicit_refs"));

  const v8::HeapGraphNode* global_object = GetGlobalObject(snapshot);
  // Use kShortcut type to skip intermediate JSGlobalPropertyCell
  const v8::HeapGraphNode* obj0 = GetProperty(
      global_object, v8::HeapGraphEdge::kShortcut, "root_object");
  CHECK(obj0);
  CHECK_EQ(v8::HeapGraphNode::kObject, obj0->GetType());
  const v8::HeapGraphNode* obj1 = GetProperty(
      obj0, v8::HeapGraphEdge::kInternal, "native");
  CHECK(obj1);
  int implicit_targets_count = 0;
  for (int i = 0, count = obj1->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = obj1->GetChild(i);
    v8::String::AsciiValue prop_name(prop->GetName());
    if (prop->GetType() == v8::HeapGraphEdge::kInternal &&
        strcmp("native", *prop_name) == 0) {
      ++implicit_targets_count;
    }
  }
  CHECK_EQ(2, implicit_targets_count);
  v8::V8::SetGlobalGCPrologueCallback(NULL);
}


TEST(DeleteAllHeapSnapshots) {
  v8::HandleScope scope;
  LocalContext env;

  CHECK_EQ(0, v8::HeapProfiler::GetSnapshotsCount());
  v8::HeapProfiler::DeleteAllSnapshots();
  CHECK_EQ(0, v8::HeapProfiler::GetSnapshotsCount());
  CHECK_NE(NULL, v8::HeapProfiler::TakeSnapshot(v8_str("1")));
  CHECK_EQ(1, v8::HeapProfiler::GetSnapshotsCount());
  v8::HeapProfiler::DeleteAllSnapshots();
  CHECK_EQ(0, v8::HeapProfiler::GetSnapshotsCount());
  CHECK_NE(NULL, v8::HeapProfiler::TakeSnapshot(v8_str("1")));
  CHECK_NE(NULL, v8::HeapProfiler::TakeSnapshot(v8_str("2")));
  CHECK_EQ(2, v8::HeapProfiler::GetSnapshotsCount());
  v8::HeapProfiler::DeleteAllSnapshots();
  CHECK_EQ(0, v8::HeapProfiler::GetSnapshotsCount());
}


TEST(DeleteHeapSnapshot) {
  v8::HandleScope scope;
  LocalContext env;

  CHECK_EQ(0, v8::HeapProfiler::GetSnapshotsCount());
  const v8::HeapSnapshot* s1 =
      v8::HeapProfiler::TakeSnapshot(v8_str("1"));
  CHECK_NE(NULL, s1);
  CHECK_EQ(1, v8::HeapProfiler::GetSnapshotsCount());
  unsigned uid1 = s1->GetUid();
  CHECK_EQ(s1, v8::HeapProfiler::FindSnapshot(uid1));
  const_cast<v8::HeapSnapshot*>(s1)->Delete();
  CHECK_EQ(0, v8::HeapProfiler::GetSnapshotsCount());
  CHECK_EQ(NULL, v8::HeapProfiler::FindSnapshot(uid1));

  const v8::HeapSnapshot* s2 =
      v8::HeapProfiler::TakeSnapshot(v8_str("2"));
  CHECK_NE(NULL, s2);
  CHECK_EQ(1, v8::HeapProfiler::GetSnapshotsCount());
  unsigned uid2 = s2->GetUid();
  CHECK_NE(static_cast<int>(uid1), static_cast<int>(uid2));
  CHECK_EQ(s2, v8::HeapProfiler::FindSnapshot(uid2));
  const v8::HeapSnapshot* s3 =
      v8::HeapProfiler::TakeSnapshot(v8_str("3"));
  CHECK_NE(NULL, s3);
  CHECK_EQ(2, v8::HeapProfiler::GetSnapshotsCount());
  unsigned uid3 = s3->GetUid();
  CHECK_NE(static_cast<int>(uid1), static_cast<int>(uid3));
  CHECK_EQ(s3, v8::HeapProfiler::FindSnapshot(uid3));
  const_cast<v8::HeapSnapshot*>(s2)->Delete();
  CHECK_EQ(1, v8::HeapProfiler::GetSnapshotsCount());
  CHECK_EQ(NULL, v8::HeapProfiler::FindSnapshot(uid2));
  CHECK_EQ(s3, v8::HeapProfiler::FindSnapshot(uid3));
  const_cast<v8::HeapSnapshot*>(s3)->Delete();
  CHECK_EQ(0, v8::HeapProfiler::GetSnapshotsCount());
  CHECK_EQ(NULL, v8::HeapProfiler::FindSnapshot(uid3));
}


TEST(DocumentURL) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun("document = { URL:\"abcdefgh\" };");

  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("document"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  CHECK_EQ("Object / abcdefgh",
           const_cast<i::HeapEntry*>(
               reinterpret_cast<const i::HeapEntry*>(global))->name());
}


TEST(DocumentWithException) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "this.__defineGetter__(\"document\", function() { throw new Error(); })");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("document"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  CHECK_EQ("Object",
           const_cast<i::HeapEntry*>(
               reinterpret_cast<const i::HeapEntry*>(global))->name());
}


TEST(DocumentURLWithException) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function URLWithException() {}\n"
      "URLWithException.prototype = { get URL() { throw new Error(); } };\n"
      "document = { URL: new URLWithException() };");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("document"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  CHECK_EQ("Object",
           const_cast<i::HeapEntry*>(
               reinterpret_cast<const i::HeapEntry*>(global))->name());
}


TEST(NoHandleLeaks) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun("document = { URL:\"abcdefgh\" };");

  v8::Handle<v8::String> name(v8_str("leakz"));
  int count_before = i::HandleScope::NumberOfHandles();
  v8::HeapProfiler::TakeSnapshot(name);
  int count_after = i::HandleScope::NumberOfHandles();
  CHECK_EQ(count_before, count_after);
}


TEST(NodesIteration) {
  v8::HandleScope scope;
  LocalContext env;
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("iteration"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  // Verify that we can find this object by iteration.
  const int nodes_count = snapshot->GetNodesCount();
  int count = 0;
  for (int i = 0; i < nodes_count; ++i) {
    if (snapshot->GetNode(i) == global)
      ++count;
  }
  CHECK_EQ(1, count);
}


TEST(GetHeapValue) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun("a = { s_prop: \'value\', n_prop: 0.1 };");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("value"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(global->GetHeapValue()->IsObject());
  v8::Local<v8::Object> js_global =
      env->Global()->GetPrototype().As<v8::Object>();
  CHECK(js_global == global->GetHeapValue());
  const v8::HeapGraphNode* obj = GetProperty(
      global, v8::HeapGraphEdge::kShortcut, "a");
  CHECK(obj->GetHeapValue()->IsObject());
  v8::Local<v8::Object> js_obj = js_global->Get(v8_str("a")).As<v8::Object>();
  CHECK(js_obj == obj->GetHeapValue());
  const v8::HeapGraphNode* s_prop =
      GetProperty(obj, v8::HeapGraphEdge::kProperty, "s_prop");
  v8::Local<v8::String> js_s_prop =
      js_obj->Get(v8_str("s_prop")).As<v8::String>();
  CHECK(js_s_prop == s_prop->GetHeapValue());
  const v8::HeapGraphNode* n_prop =
      GetProperty(obj, v8::HeapGraphEdge::kProperty, "n_prop");
  v8::Local<v8::Number> js_n_prop =
      js_obj->Get(v8_str("n_prop")).As<v8::Number>();
  CHECK(js_n_prop == n_prop->GetHeapValue());
}


TEST(GetHeapValueForDeletedObject) {
  v8::HandleScope scope;
  LocalContext env;

  // It is impossible to delete a global property, so we are about to delete a
  // property of the "a" object. Also, the "p" object can't be an empty one
  // because the empty object is static and isn't actually deleted.
  CompileRun("a = { p: { r: {} } };");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("snapshot"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* obj = GetProperty(
      global, v8::HeapGraphEdge::kShortcut, "a");
  const v8::HeapGraphNode* prop = GetProperty(
      obj, v8::HeapGraphEdge::kProperty, "p");
  {
    // Perform the check inside a nested local scope to avoid creating a
    // reference to the object we are deleting.
    v8::HandleScope scope;
    CHECK(prop->GetHeapValue()->IsObject());
  }
  CompileRun("delete a.p;");
  CHECK(prop->GetHeapValue()->IsUndefined());
}


static int StringCmp(const char* ref, i::String* act) {
  i::SmartArrayPointer<char> s_act = act->ToCString();
  int result = strcmp(ref, *s_act);
  if (result != 0)
    fprintf(stderr, "Expected: \"%s\", Actual: \"%s\"\n", ref, *s_act);
  return result;
}


TEST(GetConstructorName) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function Constructor1() {};\n"
      "var obj1 = new Constructor1();\n"
      "var Constructor2 = function() {};\n"
      "var obj2 = new Constructor2();\n"
      "var obj3 = {};\n"
      "obj3.constructor = function Constructor3() {};\n"
      "var obj4 = {};\n"
      "// Slow properties\n"
      "for (var i=0; i<2000; ++i) obj4[\"p\" + i] = i;\n"
      "obj4.constructor = function Constructor4() {};\n"
      "var obj5 = {};\n"
      "var obj6 = {};\n"
      "obj6.constructor = 6;");
  v8::Local<v8::Object> js_global =
      env->Global()->GetPrototype().As<v8::Object>();
  v8::Local<v8::Object> obj1 = js_global->Get(v8_str("obj1")).As<v8::Object>();
  i::Handle<i::JSObject> js_obj1 = v8::Utils::OpenHandle(*obj1);
  CHECK_EQ(0, StringCmp(
      "Constructor1", i::V8HeapExplorer::GetConstructorName(*js_obj1)));
  v8::Local<v8::Object> obj2 = js_global->Get(v8_str("obj2")).As<v8::Object>();
  i::Handle<i::JSObject> js_obj2 = v8::Utils::OpenHandle(*obj2);
  CHECK_EQ(0, StringCmp(
      "Constructor2", i::V8HeapExplorer::GetConstructorName(*js_obj2)));
  v8::Local<v8::Object> obj3 = js_global->Get(v8_str("obj3")).As<v8::Object>();
  i::Handle<i::JSObject> js_obj3 = v8::Utils::OpenHandle(*obj3);
  CHECK_EQ(0, StringCmp(
      "Constructor3", i::V8HeapExplorer::GetConstructorName(*js_obj3)));
  v8::Local<v8::Object> obj4 = js_global->Get(v8_str("obj4")).As<v8::Object>();
  i::Handle<i::JSObject> js_obj4 = v8::Utils::OpenHandle(*obj4);
  CHECK_EQ(0, StringCmp(
      "Constructor4", i::V8HeapExplorer::GetConstructorName(*js_obj4)));
  v8::Local<v8::Object> obj5 = js_global->Get(v8_str("obj5")).As<v8::Object>();
  i::Handle<i::JSObject> js_obj5 = v8::Utils::OpenHandle(*obj5);
  CHECK_EQ(0, StringCmp(
      "Object", i::V8HeapExplorer::GetConstructorName(*js_obj5)));
  v8::Local<v8::Object> obj6 = js_global->Get(v8_str("obj6")).As<v8::Object>();
  i::Handle<i::JSObject> js_obj6 = v8::Utils::OpenHandle(*obj6);
  CHECK_EQ(0, StringCmp(
      "Object", i::V8HeapExplorer::GetConstructorName(*js_obj6)));
}


TEST(FastCaseGetter) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun("var obj1 = {};\n"
             "obj1.__defineGetter__('propWithGetter', function Y() {\n"
             "  return 42;\n"
             "});\n"
             "obj1.__defineSetter__('propWithSetter', function Z(value) {\n"
             "  return this.value_ = value;\n"
             "});\n");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("fastCaseGetter"));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  const v8::HeapGraphNode* obj1 =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "obj1");
  CHECK_NE(NULL, obj1);
  const v8::HeapGraphNode* getterFunction =
      GetProperty(obj1, v8::HeapGraphEdge::kProperty, "get-propWithGetter");
  CHECK_NE(NULL, getterFunction);
  const v8::HeapGraphNode* setterFunction =
      GetProperty(obj1, v8::HeapGraphEdge::kProperty, "set-propWithSetter");
  CHECK_NE(NULL, setterFunction);
}


bool HasWeakEdge(const v8::HeapGraphNode* node) {
  for (int i = 0; i < node->GetChildrenCount(); ++i) {
    const v8::HeapGraphEdge* handle_edge = node->GetChild(i);
    if (handle_edge->GetType() == v8::HeapGraphEdge::kWeak) return true;
  }
  return false;
}


bool HasWeakGlobalHandle() {
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("weaks"));
  const v8::HeapGraphNode* gc_roots = GetNode(
      snapshot->GetRoot(), v8::HeapGraphNode::kObject, "(GC roots)");
  CHECK_NE(NULL, gc_roots);
  const v8::HeapGraphNode* global_handles = GetNode(
      gc_roots, v8::HeapGraphNode::kObject, "(Global handles)");
  CHECK_NE(NULL, global_handles);
  return HasWeakEdge(global_handles);
}


static void PersistentHandleCallback(v8::Persistent<v8::Value> handle, void*) {
  handle.Dispose();
}


TEST(WeakGlobalHandle) {
  v8::HandleScope scope;
  LocalContext env;

  CHECK(!HasWeakGlobalHandle());

  v8::Persistent<v8::Object> handle =
      v8::Persistent<v8::Object>::New(v8::Object::New());
  handle.MakeWeak(NULL, PersistentHandleCallback);

  CHECK(HasWeakGlobalHandle());
}


TEST(WeakGlobalContextRefs) {
  v8::HandleScope scope;
  LocalContext env;

  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("weaks"));
  const v8::HeapGraphNode* gc_roots = GetNode(
      snapshot->GetRoot(), v8::HeapGraphNode::kObject, "(GC roots)");
  CHECK_NE(NULL, gc_roots);
  const v8::HeapGraphNode* global_handles = GetNode(
      gc_roots, v8::HeapGraphNode::kObject, "(Global handles)");
  CHECK_NE(NULL, global_handles);
  const v8::HeapGraphNode* global_context = GetNode(
      global_handles, v8::HeapGraphNode::kHidden, "system / GlobalContext");
  CHECK_NE(NULL, global_context);
  CHECK(HasWeakEdge(global_context));
}


TEST(SfiAndJsFunctionWeakRefs) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "fun = (function (x) { return function () { return x + 1; } })(1);");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8_str("fun"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  const v8::HeapGraphNode* fun =
      GetProperty(global, v8::HeapGraphEdge::kShortcut, "fun");
  CHECK(HasWeakEdge(fun));
  const v8::HeapGraphNode* shared =
      GetProperty(fun, v8::HeapGraphEdge::kInternal, "shared");
  CHECK(HasWeakEdge(shared));
}


TEST(PersistentHandleCount) {
  v8::HandleScope scope;
  LocalContext env;

  // V8 also uses global handles internally, so we can't test for an absolute
  // number.
  int global_handle_count = v8::HeapProfiler::GetPersistentHandleCount();

  // Create some persistent handles.
  v8::Persistent<v8::String> p_AAA =
      v8::Persistent<v8::String>::New(v8_str("AAA"));
  CHECK_EQ(global_handle_count + 1,
           v8::HeapProfiler::GetPersistentHandleCount());
  v8::Persistent<v8::String> p_BBB =
      v8::Persistent<v8::String>::New(v8_str("BBB"));
  CHECK_EQ(global_handle_count + 2,
           v8::HeapProfiler::GetPersistentHandleCount());
  v8::Persistent<v8::String> p_CCC =
      v8::Persistent<v8::String>::New(v8_str("CCC"));
  CHECK_EQ(global_handle_count + 3,
           v8::HeapProfiler::GetPersistentHandleCount());

  // Dipose the persistent handles in a different order.
  p_AAA.Dispose();
  CHECK_EQ(global_handle_count + 2,
           v8::HeapProfiler::GetPersistentHandleCount());
  p_CCC.Dispose();
  CHECK_EQ(global_handle_count + 1,
           v8::HeapProfiler::GetPersistentHandleCount());
  p_BBB.Dispose();
  CHECK_EQ(global_handle_count, v8::HeapProfiler::GetPersistentHandleCount());
}
