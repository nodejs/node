// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Tests for heap profiler

#include <ctype.h>

#include <memory>

#include "src/v8.h"

#include "include/v8-profiler.h"
#include "src/api.h"
#include "src/base/hashmap.h"
#include "src/collector.h"
#include "src/debug/debug.h"
#include "src/objects-inl.h"
#include "src/profiler/allocation-tracker.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/heap-snapshot-generator-inl.h"
#include "test/cctest/cctest.h"

using i::AllocationTraceNode;
using i::AllocationTraceTree;
using i::AllocationTracker;
using i::ArrayVector;
using i::Vector;

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
    v8::base::HashMap visited;
    std::vector<i::HeapEntry*> list;
    list.push_back(root);
    CheckEntry(root);
    while (!list.empty()) {
      i::HeapEntry* entry = list.back();
      list.pop_back();
      for (int i = 0; i < entry->children_count(); ++i) {
        i::HeapGraphEdge* edge = entry->child(i);
        if (edge->type() == i::HeapGraphEdge::kShortcut) continue;
        i::HeapEntry* child = edge->to();
        v8::base::HashMap::Entry* entry = visited.LookupOrInsert(
            reinterpret_cast<void*>(child),
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(child)));
        if (entry->value)
          continue;
        entry->value = reinterpret_cast<void*>(1);
        list.push_back(child);
        CheckEntry(child);
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
  // The 0th-child is (GC Roots), 1st is the user root.
  const v8::HeapGraphNode* global_obj =
      snapshot->GetRoot()->GetChild(1)->GetToNode();
  CHECK_EQ(0, strncmp("Object", const_cast<i::HeapEntry*>(
      reinterpret_cast<const i::HeapEntry*>(global_obj))->name(), 6));
  return global_obj;
}

static const v8::HeapGraphNode* GetProperty(v8::Isolate* isolate,
                                            const v8::HeapGraphNode* node,
                                            v8::HeapGraphEdge::Type type,
                                            const char* name) {
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = node->GetChild(i);
    v8::String::Utf8Value prop_name(isolate, prop->GetName());
    if (prop->GetType() == type && strcmp(name, *prop_name) == 0)
      return prop->GetToNode();
  }
  return nullptr;
}

static bool HasString(v8::Isolate* isolate, const v8::HeapGraphNode* node,
                      const char* contents) {
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = node->GetChild(i);
    const v8::HeapGraphNode* node = prop->GetToNode();
    if (node->GetType() == v8::HeapGraphNode::kString) {
      v8::String::Utf8Value node_name(isolate, node->GetName());
      if (strcmp(contents, *node_name) == 0) return true;
    }
  }
  return false;
}

static void EnsureNoUninstrumentedInternals(v8::Isolate* isolate,
                                            const v8::HeapGraphNode* node) {
  for (int i = 0; i < 20; ++i) {
    i::ScopedVector<char> buffer(10);
    const v8::HeapGraphNode* internal =
        GetProperty(isolate, node, v8::HeapGraphEdge::kInternal,
                    i::IntToCString(i, buffer));
    CHECK(!internal);
  }
}

// Check that snapshot has no unretained entries except root.
static bool ValidateSnapshot(const v8::HeapSnapshot* snapshot, int depth = 3) {
  i::HeapSnapshot* heap_snapshot = const_cast<i::HeapSnapshot*>(
      reinterpret_cast<const i::HeapSnapshot*>(snapshot));

  v8::base::HashMap visited;
  std::deque<i::HeapGraphEdge>& edges = heap_snapshot->edges();
  for (size_t i = 0; i < edges.size(); ++i) {
    v8::base::HashMap::Entry* entry = visited.LookupOrInsert(
        reinterpret_cast<void*>(edges[i].to()),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(edges[i].to())));
    uint32_t ref_count = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(entry->value));
    entry->value = reinterpret_cast<void*>(ref_count + 1);
  }
  uint32_t unretained_entries_count = 0;
  std::vector<i::HeapEntry>& entries = heap_snapshot->entries();
  for (i::HeapEntry& entry : entries) {
    v8::base::HashMap::Entry* map_entry = visited.Lookup(
        reinterpret_cast<void*>(&entry),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&entry)));
    if (!map_entry && entry.id() != 1) {
      entry.Print("entry with no retainer", "", depth, 0);
      ++unretained_entries_count;
    }
  }
  return unretained_entries_count == 0;
}


TEST(HeapSnapshot) {
  LocalContext env2;
  v8::HandleScope scope(env2->GetIsolate());
  v8::HeapProfiler* heap_profiler = env2->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function A2() {}\n"
      "function B2(x) { return function() { return typeof x; }; }\n"
      "function C2(x) { this.x1 = x; this.x2 = x; this[1] = x; }\n"
      "var a2 = new A2();\n"
      "var b2_1 = new B2(a2), b2_2 = new B2(a2);\n"
      "var c2 = new C2(a2);");
  const v8::HeapSnapshot* snapshot_env2 = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot_env2));
  const v8::HeapGraphNode* global_env2 = GetGlobalObject(snapshot_env2);

  // Verify, that JS global object of env2 has '..2' properties.
  const v8::HeapGraphNode* a2_node = GetProperty(
      env2->GetIsolate(), global_env2, v8::HeapGraphEdge::kProperty, "a2");
  CHECK(a2_node);
  CHECK(GetProperty(env2->GetIsolate(), global_env2,
                    v8::HeapGraphEdge::kProperty, "b2_1"));
  CHECK(GetProperty(env2->GetIsolate(), global_env2,
                    v8::HeapGraphEdge::kProperty, "b2_2"));
  CHECK(GetProperty(env2->GetIsolate(), global_env2,
                    v8::HeapGraphEdge::kProperty, "c2"));

  NamedEntriesDetector det;
  det.CheckAllReachables(const_cast<i::HeapEntry*>(
      reinterpret_cast<const i::HeapEntry*>(global_env2)));
  CHECK(det.has_A2);
  CHECK(det.has_B2);
  CHECK(det.has_C2);
}


TEST(HeapSnapshotObjectSizes) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  //   -a-> X1 --a
  // x -b-> X2 <-|
  CompileRun(
      "function X(a, b) { this.a = a; this.b = b; }\n"
      "x = new X(new X(), new X());\n"
      "dummy = new X();\n"
      "(function() { x.a.a = x.b; })();");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* x =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "x");
  CHECK(x);
  const v8::HeapGraphNode* x1 =
      GetProperty(env->GetIsolate(), x, v8::HeapGraphEdge::kProperty, "a");
  CHECK(x1);
  const v8::HeapGraphNode* x2 =
      GetProperty(env->GetIsolate(), x, v8::HeapGraphEdge::kProperty, "b");
  CHECK(x2);

  // Test sizes.
  CHECK_NE(0, static_cast<int>(x->GetShallowSize()));
  CHECK_NE(0, static_cast<int>(x1->GetShallowSize()));
  CHECK_NE(0, static_cast<int>(x2->GetShallowSize()));
}


TEST(BoundFunctionInSnapshot) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun(
      "function myFunction(a, b) { this.a = a; this.b = b; }\n"
      "function AAAAA() {}\n"
      "boundFunction = myFunction.bind(new AAAAA(), 20, new Number(12)); \n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* f = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "boundFunction");
  CHECK(f);
  CHECK(v8_str("native_bind")->Equals(env.local(), f->GetName()).FromJust());
  const v8::HeapGraphNode* bindings = GetProperty(
      env->GetIsolate(), f, v8::HeapGraphEdge::kInternal, "bindings");
  CHECK(bindings);
  CHECK_EQ(v8::HeapGraphNode::kArray, bindings->GetType());
  CHECK_EQ(1, bindings->GetChildrenCount());

  const v8::HeapGraphNode* bound_this = GetProperty(
      env->GetIsolate(), f, v8::HeapGraphEdge::kInternal, "bound_this");
  CHECK(bound_this);
  CHECK_EQ(v8::HeapGraphNode::kObject, bound_this->GetType());

  const v8::HeapGraphNode* bound_function = GetProperty(
      env->GetIsolate(), f, v8::HeapGraphEdge::kInternal, "bound_function");
  CHECK(bound_function);
  CHECK_EQ(v8::HeapGraphNode::kClosure, bound_function->GetType());

  const v8::HeapGraphNode* bound_argument = GetProperty(
      env->GetIsolate(), f, v8::HeapGraphEdge::kShortcut, "bound_argument_1");
  CHECK(bound_argument);
  CHECK_EQ(v8::HeapGraphNode::kObject, bound_argument->GetType());
}


TEST(HeapSnapshotEntryChildren) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function A() { }\n"
      "a = new A;");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  for (int i = 0, count = global->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = global->GetChild(i);
    CHECK_EQ(global, prop->GetFromNode());
  }
  const v8::HeapGraphNode* a =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "a");
  CHECK(a);
  for (int i = 0, count = a->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = a->GetChild(i);
    CHECK_EQ(a, prop->GetFromNode());
  }
}


TEST(HeapSnapshotCodeObjects) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function lazy(x) { return x - 1; }\n"
      "function compiled(x) { return x + 1; }\n"
      "var anonymous = (function() { return function() { return 0; } })();\n"
      "compiled(1)");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* compiled = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "compiled");
  CHECK(compiled);
  CHECK_EQ(v8::HeapGraphNode::kClosure, compiled->GetType());
  const v8::HeapGraphNode* lazy = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "lazy");
  CHECK(lazy);
  CHECK_EQ(v8::HeapGraphNode::kClosure, lazy->GetType());
  const v8::HeapGraphNode* anonymous = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "anonymous");
  CHECK(anonymous);
  CHECK_EQ(v8::HeapGraphNode::kClosure, anonymous->GetType());
  v8::String::Utf8Value anonymous_name(env->GetIsolate(), anonymous->GetName());
  CHECK_EQ(0, strcmp("", *anonymous_name));

  // Find references to code.
  const v8::HeapGraphNode* compiled_code = GetProperty(
      env->GetIsolate(), compiled, v8::HeapGraphEdge::kInternal, "shared");
  CHECK(compiled_code);
  const v8::HeapGraphNode* lazy_code = GetProperty(
      env->GetIsolate(), lazy, v8::HeapGraphEdge::kInternal, "shared");
  CHECK(lazy_code);

  // Check that there's no strong next_code_link. There might be a weak one
  // but might be not, so we can't check that fact.
  const v8::HeapGraphNode* code = GetProperty(
      env->GetIsolate(), compiled_code, v8::HeapGraphEdge::kInternal, "code");
  CHECK(code);
  const v8::HeapGraphNode* next_code_link = GetProperty(
      env->GetIsolate(), code, v8::HeapGraphEdge::kInternal, "code");
  CHECK(!next_code_link);

  // Verify that non-compiled code doesn't contain references to "x"
  // literal, while compiled code does. The scope info is stored in FixedArray
  // objects attached to the SharedFunctionInfo.
  bool compiled_references_x = false, lazy_references_x = false;
  for (int i = 0, count = compiled_code->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = compiled_code->GetChild(i);
    const v8::HeapGraphNode* node = prop->GetToNode();
    if (node->GetType() == v8::HeapGraphNode::kArray) {
      if (HasString(env->GetIsolate(), node, "x")) {
        compiled_references_x = true;
        break;
      }
    }
  }
  for (int i = 0, count = lazy_code->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = lazy_code->GetChild(i);
    const v8::HeapGraphNode* node = prop->GetToNode();
    if (node->GetType() == v8::HeapGraphNode::kArray) {
      if (HasString(env->GetIsolate(), node, "x")) {
        lazy_references_x = true;
        break;
      }
    }
  }
  CHECK(compiled_references_x);
  if (i::FLAG_lazy) {
    CHECK(!lazy_references_x);
  }
}


TEST(HeapSnapshotHeapNumbers) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun(
      "a = 1;    // a is Smi\n"
      "b = 2.5;  // b is HeapNumber");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(!GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty,
                     "a"));
  const v8::HeapGraphNode* b =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "b");
  CHECK(b);
  CHECK_EQ(v8::HeapGraphNode::kHeapNumber, b->GetType());
}


TEST(HeapSnapshotSlicedString) {
  if (!i::FLAG_string_slices) return;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun(
      "parent_string = \"123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789.\";"
      "child_string = parent_string.slice(100);");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* parent_string = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "parent_string");
  CHECK(parent_string);
  const v8::HeapGraphNode* child_string = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "child_string");
  CHECK(child_string);
  CHECK_EQ(v8::HeapGraphNode::kSlicedString, child_string->GetType());
  const v8::HeapGraphNode* parent = GetProperty(
      env->GetIsolate(), child_string, v8::HeapGraphEdge::kInternal, "parent");
  CHECK_EQ(parent_string, parent);
  heap_profiler->DeleteAllHeapSnapshots();
}


TEST(HeapSnapshotConsString) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->SetInternalFieldCount(1);
  LocalContext env(nullptr, global_template);
  v8::Local<v8::Object> global_proxy = env->Global();
  v8::Local<v8::Object> global = global_proxy->GetPrototype().As<v8::Object>();
  CHECK_EQ(1, global->InternalFieldCount());

  i::Factory* factory = CcTest::i_isolate()->factory();
  i::Handle<i::String> first = factory->NewStringFromStaticChars("0123456789");
  i::Handle<i::String> second = factory->NewStringFromStaticChars("0123456789");
  i::Handle<i::String> cons_string =
      factory->NewConsString(first, second).ToHandleChecked();

  global->SetInternalField(0, v8::ToApiHandle<v8::String>(cons_string));

  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global_node = GetGlobalObject(snapshot);

  const v8::HeapGraphNode* string_node =
      GetProperty(isolate, global_node, v8::HeapGraphEdge::kInternal, "0");
  CHECK(string_node);
  CHECK_EQ(v8::HeapGraphNode::kConsString, string_node->GetType());

  const v8::HeapGraphNode* first_node =
      GetProperty(isolate, string_node, v8::HeapGraphEdge::kInternal, "first");
  CHECK_EQ(v8::HeapGraphNode::kString, first_node->GetType());

  const v8::HeapGraphNode* second_node =
      GetProperty(isolate, string_node, v8::HeapGraphEdge::kInternal, "second");
  CHECK_EQ(v8::HeapGraphNode::kString, second_node->GetType());

  heap_profiler->DeleteAllHeapSnapshots();
}


TEST(HeapSnapshotSymbol) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("a = Symbol('mySymbol');\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* a =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "a");
  CHECK(a);
  CHECK_EQ(a->GetType(), v8::HeapGraphNode::kSymbol);
  CHECK(v8_str("symbol")->Equals(env.local(), a->GetName()).FromJust());
  const v8::HeapGraphNode* name =
      GetProperty(env->GetIsolate(), a, v8::HeapGraphEdge::kInternal, "name");
  CHECK(name);
  CHECK(v8_str("mySymbol")->Equals(env.local(), name->GetName()).FromJust());
}

TEST(HeapSnapshotWeakCollection) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "k = {}; v = {}; s = 'str';\n"
      "ws = new WeakSet(); ws.add(k); ws.add(v); ws[s] = s;\n"
      "wm = new WeakMap(); wm.set(k, v); wm[s] = s;\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* k =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "k");
  CHECK(k);
  const v8::HeapGraphNode* v =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "v");
  CHECK(v);
  const v8::HeapGraphNode* s =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "s");
  CHECK(s);

  const v8::HeapGraphNode* ws = GetProperty(env->GetIsolate(), global,
                                            v8::HeapGraphEdge::kProperty, "ws");
  CHECK(ws);
  CHECK_EQ(v8::HeapGraphNode::kObject, ws->GetType());
  CHECK(v8_str("WeakSet")->Equals(env.local(), ws->GetName()).FromJust());

  const v8::HeapGraphNode* ws_table =
      GetProperty(env->GetIsolate(), ws, v8::HeapGraphEdge::kInternal, "table");
  CHECK_EQ(v8::HeapGraphNode::kArray, ws_table->GetType());
  CHECK_GT(ws_table->GetChildrenCount(), 0);
  int weak_entries = 0;
  for (int i = 0, count = ws_table->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = ws_table->GetChild(i);
    if (prop->GetType() != v8::HeapGraphEdge::kWeak) continue;
    if (k->GetId() == prop->GetToNode()->GetId()) {
      ++weak_entries;
    }
  }
  CHECK_EQ(1, weak_entries);
  const v8::HeapGraphNode* ws_s =
      GetProperty(env->GetIsolate(), ws, v8::HeapGraphEdge::kProperty, "str");
  CHECK(ws_s);
  CHECK_EQ(s->GetId(), ws_s->GetId());

  const v8::HeapGraphNode* wm = GetProperty(env->GetIsolate(), global,
                                            v8::HeapGraphEdge::kProperty, "wm");
  CHECK(wm);
  CHECK_EQ(v8::HeapGraphNode::kObject, wm->GetType());
  CHECK(v8_str("WeakMap")->Equals(env.local(), wm->GetName()).FromJust());

  const v8::HeapGraphNode* wm_table =
      GetProperty(env->GetIsolate(), wm, v8::HeapGraphEdge::kInternal, "table");
  CHECK_EQ(v8::HeapGraphNode::kArray, wm_table->GetType());
  CHECK_GT(wm_table->GetChildrenCount(), 0);
  weak_entries = 0;
  for (int i = 0, count = wm_table->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = wm_table->GetChild(i);
    if (prop->GetType() != v8::HeapGraphEdge::kWeak) continue;
    const v8::SnapshotObjectId to_node_id = prop->GetToNode()->GetId();
    if (to_node_id == k->GetId() || to_node_id == v->GetId()) {
      ++weak_entries;
    }
  }
  CHECK_EQ(1, weak_entries);  // Key is the only weak.
  const v8::HeapGraphNode* wm_s =
      GetProperty(env->GetIsolate(), wm, v8::HeapGraphEdge::kProperty, "str");
  CHECK(wm_s);
  CHECK_EQ(s->GetId(), wm_s->GetId());
}


TEST(HeapSnapshotCollection) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "k = {}; v = {}; s = 'str';\n"
      "set = new Set(); set.add(k); set.add(v); set[s] = s;\n"
      "map = new Map(); map.set(k, v); map[s] = s;\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* k =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "k");
  CHECK(k);
  const v8::HeapGraphNode* v =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "v");
  CHECK(v);
  const v8::HeapGraphNode* s =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "s");
  CHECK(s);

  const v8::HeapGraphNode* set = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "set");
  CHECK(set);
  CHECK_EQ(v8::HeapGraphNode::kObject, set->GetType());
  CHECK(v8_str("Set")->Equals(env.local(), set->GetName()).FromJust());

  const v8::HeapGraphNode* set_table = GetProperty(
      env->GetIsolate(), set, v8::HeapGraphEdge::kInternal, "table");
  CHECK_EQ(v8::HeapGraphNode::kArray, set_table->GetType());
  CHECK_GT(set_table->GetChildrenCount(), 0);
  int entries = 0;
  for (int i = 0, count = set_table->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = set_table->GetChild(i);
    const v8::SnapshotObjectId to_node_id = prop->GetToNode()->GetId();
    if (to_node_id == k->GetId() || to_node_id == v->GetId()) {
      ++entries;
    }
  }
  CHECK_EQ(2, entries);
  const v8::HeapGraphNode* set_s =
      GetProperty(env->GetIsolate(), set, v8::HeapGraphEdge::kProperty, "str");
  CHECK(set_s);
  CHECK_EQ(s->GetId(), set_s->GetId());

  const v8::HeapGraphNode* map = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "map");
  CHECK(map);
  CHECK_EQ(v8::HeapGraphNode::kObject, map->GetType());
  CHECK(v8_str("Map")->Equals(env.local(), map->GetName()).FromJust());

  const v8::HeapGraphNode* map_table = GetProperty(
      env->GetIsolate(), map, v8::HeapGraphEdge::kInternal, "table");
  CHECK_EQ(v8::HeapGraphNode::kArray, map_table->GetType());
  CHECK_GT(map_table->GetChildrenCount(), 0);
  entries = 0;
  for (int i = 0, count = map_table->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = map_table->GetChild(i);
    const v8::SnapshotObjectId to_node_id = prop->GetToNode()->GetId();
    if (to_node_id == k->GetId() || to_node_id == v->GetId()) {
      ++entries;
    }
  }
  CHECK_EQ(2, entries);
  const v8::HeapGraphNode* map_s =
      GetProperty(env->GetIsolate(), map, v8::HeapGraphEdge::kProperty, "str");
  CHECK(map_s);
  CHECK_EQ(s->GetId(), map_s->GetId());
}

TEST(HeapSnapshotMap) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function Z() { this.foo = {}; }\n"
      "z = new Z();\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* z =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "z");
  CHECK(z);
  const v8::HeapGraphNode* map =
      GetProperty(env->GetIsolate(), z, v8::HeapGraphEdge::kInternal, "map");
  CHECK(map);
  CHECK(
      GetProperty(env->GetIsolate(), map, v8::HeapGraphEdge::kInternal, "map"));
  CHECK(GetProperty(env->GetIsolate(), map, v8::HeapGraphEdge::kInternal,
                    "prototype"));
  CHECK(GetProperty(env->GetIsolate(), map, v8::HeapGraphEdge::kInternal,
                    "back_pointer"));
  CHECK(GetProperty(env->GetIsolate(), map, v8::HeapGraphEdge::kInternal,
                    "descriptors"));
  const v8::HeapGraphNode* weak_cell = GetProperty(
      env->GetIsolate(), map, v8::HeapGraphEdge::kInternal, "weak_cell_cache");
  CHECK(GetProperty(env->GetIsolate(), weak_cell, v8::HeapGraphEdge::kWeak,
                    "value"));
}

TEST(HeapSnapshotInternalReferences) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->SetInternalFieldCount(2);
  LocalContext env(nullptr, global_template);
  v8::Local<v8::Object> global_proxy = env->Global();
  v8::Local<v8::Object> global = global_proxy->GetPrototype().As<v8::Object>();
  CHECK_EQ(2, global->InternalFieldCount());
  v8::Local<v8::Object> obj = v8::Object::New(isolate);
  global->SetInternalField(0, v8_num(17));
  global->SetInternalField(1, obj);
  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global_node = GetGlobalObject(snapshot);
  // The first reference will not present, because it's a Smi.
  CHECK(!GetProperty(env->GetIsolate(), global_node,
                     v8::HeapGraphEdge::kInternal, "0"));
  // The second reference is to an object.
  CHECK(GetProperty(env->GetIsolate(), global_node,
                    v8::HeapGraphEdge::kInternal, "1"));
}


TEST(HeapSnapshotAddressReuse) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function A() {}\n"
      "var a = [];\n"
      "for (var i = 0; i < 10000; ++i)\n"
      "  a[i] = new A();\n");
  const v8::HeapSnapshot* snapshot1 = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot1));
  v8::SnapshotObjectId maxId1 = snapshot1->GetMaxSnapshotJSObjectId();

  CompileRun(
      "for (var i = 0; i < 10000; ++i)\n"
      "  a[i] = new A();\n");
  CcTest::CollectAllGarbage();

  const v8::HeapSnapshot* snapshot2 = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot2));
  const v8::HeapGraphNode* global2 = GetGlobalObject(snapshot2);

  const v8::HeapGraphNode* array_node = GetProperty(
      env->GetIsolate(), global2, v8::HeapGraphEdge::kProperty, "a");
  CHECK(array_node);
  int wrong_count = 0;
  for (int i = 0, count = array_node->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = array_node->GetChild(i);
    if (prop->GetType() != v8::HeapGraphEdge::kElement)
      continue;
    v8::SnapshotObjectId id = prop->GetToNode()->GetId();
    if (id < maxId1)
      ++wrong_count;
  }
  CHECK_EQ(0, wrong_count);
}


TEST(HeapEntryIdsAndArrayShift) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function AnObject() {\n"
      "    this.first = 'first';\n"
      "    this.second = 'second';\n"
      "}\n"
      "var a = new Array();\n"
      "for (var i = 0; i < 10; ++i)\n"
      "  a.push(new AnObject());\n");
  const v8::HeapSnapshot* snapshot1 = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot1));

  CompileRun(
      "for (var i = 0; i < 1; ++i)\n"
      "  a.shift();\n");

  CcTest::CollectAllGarbage();

  const v8::HeapSnapshot* snapshot2 = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot2));

  const v8::HeapGraphNode* global1 = GetGlobalObject(snapshot1);
  const v8::HeapGraphNode* global2 = GetGlobalObject(snapshot2);
  CHECK_NE(0u, global1->GetId());
  CHECK_EQ(global1->GetId(), global2->GetId());

  const v8::HeapGraphNode* a1 = GetProperty(env->GetIsolate(), global1,
                                            v8::HeapGraphEdge::kProperty, "a");
  CHECK(a1);
  const v8::HeapGraphNode* k1 = GetProperty(
      env->GetIsolate(), a1, v8::HeapGraphEdge::kInternal, "elements");
  CHECK(k1);
  const v8::HeapGraphNode* a2 = GetProperty(env->GetIsolate(), global2,
                                            v8::HeapGraphEdge::kProperty, "a");
  CHECK(a2);
  const v8::HeapGraphNode* k2 = GetProperty(
      env->GetIsolate(), a2, v8::HeapGraphEdge::kInternal, "elements");
  CHECK(k2);

  CHECK_EQ(a1->GetId(), a2->GetId());
  CHECK_EQ(k1->GetId(), k2->GetId());
}


TEST(HeapEntryIdsAndGC) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function A() {}\n"
      "function B(x) { this.x = x; }\n"
      "var a = new A();\n"
      "var b = new B(a);");
  const v8::HeapSnapshot* snapshot1 = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot1));

  CcTest::CollectAllGarbage();

  const v8::HeapSnapshot* snapshot2 = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot2));

  CHECK_GT(snapshot1->GetMaxSnapshotJSObjectId(), 7000u);
  CHECK(snapshot1->GetMaxSnapshotJSObjectId() <=
        snapshot2->GetMaxSnapshotJSObjectId());

  const v8::HeapGraphNode* global1 = GetGlobalObject(snapshot1);
  const v8::HeapGraphNode* global2 = GetGlobalObject(snapshot2);
  CHECK_NE(0u, global1->GetId());
  CHECK_EQ(global1->GetId(), global2->GetId());
  const v8::HeapGraphNode* A1 = GetProperty(env->GetIsolate(), global1,
                                            v8::HeapGraphEdge::kProperty, "A");
  CHECK(A1);
  const v8::HeapGraphNode* A2 = GetProperty(env->GetIsolate(), global2,
                                            v8::HeapGraphEdge::kProperty, "A");
  CHECK(A2);
  CHECK_NE(0u, A1->GetId());
  CHECK_EQ(A1->GetId(), A2->GetId());
  const v8::HeapGraphNode* B1 = GetProperty(env->GetIsolate(), global1,
                                            v8::HeapGraphEdge::kProperty, "B");
  CHECK(B1);
  const v8::HeapGraphNode* B2 = GetProperty(env->GetIsolate(), global2,
                                            v8::HeapGraphEdge::kProperty, "B");
  CHECK(B2);
  CHECK_NE(0u, B1->GetId());
  CHECK_EQ(B1->GetId(), B2->GetId());
  const v8::HeapGraphNode* a1 = GetProperty(env->GetIsolate(), global1,
                                            v8::HeapGraphEdge::kProperty, "a");
  CHECK(a1);
  const v8::HeapGraphNode* a2 = GetProperty(env->GetIsolate(), global2,
                                            v8::HeapGraphEdge::kProperty, "a");
  CHECK(a2);
  CHECK_NE(0u, a1->GetId());
  CHECK_EQ(a1->GetId(), a2->GetId());
  const v8::HeapGraphNode* b1 = GetProperty(env->GetIsolate(), global1,
                                            v8::HeapGraphEdge::kProperty, "b");
  CHECK(b1);
  const v8::HeapGraphNode* b2 = GetProperty(env->GetIsolate(), global2,
                                            v8::HeapGraphEdge::kProperty, "b");
  CHECK(b2);
  CHECK_NE(0u, b1->GetId());
  CHECK_EQ(b1->GetId(), b2->GetId());
}


TEST(HeapSnapshotRootPreservedAfterSorting) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* root1 = snapshot->GetRoot();
  const_cast<i::HeapSnapshot*>(reinterpret_cast<const i::HeapSnapshot*>(
      snapshot))->GetSortedEntriesList();
  const v8::HeapGraphNode* root2 = snapshot->GetRoot();
  CHECK_EQ(root1, root2);
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
    i::MemCopy(chunk.start(), buffer, chars_written);
    return kContinue;
  }
  virtual WriteResult WriteUint32Chunk(uint32_t* buffer, int chars_written) {
    UNREACHABLE();
  }
  void WriteTo(i::Vector<char> dest) { buffer_.WriteTo(dest); }
  int eos_signaled() { return eos_signaled_; }
  int size() { return buffer_.size(); }

 private:
  i::Collector<char> buffer_;
  int eos_signaled_;
  int abort_countdown_;
};

class OneByteResource : public v8::String::ExternalOneByteStringResource {
 public:
  explicit OneByteResource(i::Vector<char> string) : data_(string.start()) {
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
  v8::Isolate* isolate = CcTest::isolate();
  LocalContext env;
  v8::HandleScope scope(isolate);
  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();

#define STRING_LITERAL_FOR_TEST \
  "\"String \\n\\r\\u0008\\u0081\\u0101\\u0801\\u8001\""
  CompileRun(
      "function A(s) { this.s = s; }\n"
      "function B(x) { this.x = x; }\n"
      "var a = new A(" STRING_LITERAL_FOR_TEST ");\n"
      "var b = new B(a);");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));

  TestJSONStream stream;
  snapshot->Serialize(&stream, v8::HeapSnapshot::kJSON);
  CHECK_GT(stream.size(), 0);
  CHECK_EQ(1, stream.eos_signaled());
  i::ScopedVector<char> json(stream.size());
  stream.WriteTo(json);

  // Verify that snapshot string is valid JSON.
  OneByteResource* json_res = new OneByteResource(json);
  v8::Local<v8::String> json_string =
      v8::String::NewExternalOneByte(env->GetIsolate(), json_res)
          .ToLocalChecked();
  env->Global()
      ->Set(env.local(), v8_str("json_snapshot"), json_string)
      .FromJust();
  v8::Local<v8::Value> snapshot_parse_result = CompileRun(
      "var parsed = JSON.parse(json_snapshot); true;");
  CHECK(!snapshot_parse_result.IsEmpty());

  // Verify that snapshot object has required fields.
  v8::Local<v8::Object> parsed_snapshot =
      env->Global()
          ->Get(env.local(), v8_str("parsed"))
          .ToLocalChecked()
          ->ToObject(env.local())
          .ToLocalChecked();
  CHECK(parsed_snapshot->Has(env.local(), v8_str("snapshot")).FromJust());
  CHECK(parsed_snapshot->Has(env.local(), v8_str("nodes")).FromJust());
  CHECK(parsed_snapshot->Has(env.local(), v8_str("edges")).FromJust());
  CHECK(parsed_snapshot->Has(env.local(), v8_str("strings")).FromJust());

  // Get node and edge "member" offsets.
  v8::Local<v8::Value> meta_analysis_result = CompileRun(
      "var meta = parsed.snapshot.meta;\n"
      "var edge_count_offset = meta.node_fields.indexOf('edge_count');\n"
      "var node_fields_count = meta.node_fields.length;\n"
      "var edge_fields_count = meta.edge_fields.length;\n"
      "var edge_type_offset = meta.edge_fields.indexOf('type');\n"
      "var edge_name_offset = meta.edge_fields.indexOf('name_or_index');\n"
      "var edge_to_node_offset = meta.edge_fields.indexOf('to_node');\n"
      "var property_type ="
      "    meta.edge_types[edge_type_offset].indexOf('property');\n"
      "var shortcut_type ="
      "    meta.edge_types[edge_type_offset].indexOf('shortcut');\n"
      "var node_count = parsed.nodes.length / node_fields_count;\n"
      "var first_edge_indexes = parsed.first_edge_indexes = [];\n"
      "for (var i = 0, first_edge_index = 0; i < node_count; ++i) {\n"
      "  first_edge_indexes[i] = first_edge_index;\n"
      "  first_edge_index += edge_fields_count *\n"
      "      parsed.nodes[i * node_fields_count + edge_count_offset];\n"
      "}\n"
      "first_edge_indexes[node_count] = first_edge_index;\n");
  CHECK(!meta_analysis_result.IsEmpty());

  // A helper function for processing encoded nodes.
  CompileRun(
      "function GetChildPosByProperty(pos, prop_name, prop_type) {\n"
      "  var nodes = parsed.nodes;\n"
      "  var edges = parsed.edges;\n"
      "  var strings = parsed.strings;\n"
      "  var node_ordinal = pos / node_fields_count;\n"
      "  for (var i = parsed.first_edge_indexes[node_ordinal],\n"
      "      count = parsed.first_edge_indexes[node_ordinal + 1];\n"
      "      i < count; i += edge_fields_count) {\n"
      "    if (edges[i + edge_type_offset] === prop_type\n"
      "        && strings[edges[i + edge_name_offset]] === prop_name)\n"
      "      return edges[i + edge_to_node_offset];\n"
      "  }\n"
      "  return null;\n"
      "}\n");
  // Get the string index using the path: <root> -> <global>.b.x.s
  v8::Local<v8::Value> string_obj_pos_val = CompileRun(
      "GetChildPosByProperty(\n"
      "  GetChildPosByProperty(\n"
      "    GetChildPosByProperty("
      "      parsed.edges[edge_fields_count + edge_to_node_offset],"
      "      \"b\", property_type),\n"
      "    \"x\", property_type),"
      "  \"s\", property_type)");
  CHECK(!string_obj_pos_val.IsEmpty());
  int string_obj_pos = static_cast<int>(
      string_obj_pos_val->ToNumber(env.local()).ToLocalChecked()->Value());
  v8::Local<v8::Object> nodes_array =
      parsed_snapshot->Get(env.local(), v8_str("nodes"))
          .ToLocalChecked()
          ->ToObject(env.local())
          .ToLocalChecked();
  int string_index =
      static_cast<int>(nodes_array->Get(env.local(), string_obj_pos + 1)
                           .ToLocalChecked()
                           ->ToNumber(env.local())
                           .ToLocalChecked()
                           ->Value());
  CHECK_GT(string_index, 0);
  v8::Local<v8::Object> strings_array =
      parsed_snapshot->Get(env.local(), v8_str("strings"))
          .ToLocalChecked()
          ->ToObject(env.local())
          .ToLocalChecked();
  v8::Local<v8::String> string = strings_array->Get(env.local(), string_index)
                                     .ToLocalChecked()
                                     ->ToString(env.local())
                                     .ToLocalChecked();
  v8::Local<v8::String> ref_string = CompileRun(STRING_LITERAL_FOR_TEST)
                                         ->ToString(env.local())
                                         .ToLocalChecked();
#undef STRING_LITERAL_FOR_TEST
  CHECK_EQ(0, strcmp(*v8::String::Utf8Value(env->GetIsolate(), ref_string),
                     *v8::String::Utf8Value(env->GetIsolate(), string)));
}


TEST(HeapSnapshotJSONSerializationAborting) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  TestJSONStream stream(5);
  snapshot->Serialize(&stream, v8::HeapSnapshot::kJSON);
  CHECK_GT(stream.size(), 0);
  CHECK_EQ(0, stream.eos_signaled());
}

namespace {

class TestStatsStream : public v8::OutputStream {
 public:
  TestStatsStream()
    : eos_signaled_(0),
      updates_written_(0),
      entries_count_(0),
      entries_size_(0),
      intervals_count_(0),
      first_interval_index_(-1) { }
  TestStatsStream(const TestStatsStream& stream)
    : v8::OutputStream(stream),
      eos_signaled_(stream.eos_signaled_),
      updates_written_(stream.updates_written_),
      entries_count_(stream.entries_count_),
      entries_size_(stream.entries_size_),
      intervals_count_(stream.intervals_count_),
      first_interval_index_(stream.first_interval_index_) { }
  virtual ~TestStatsStream() {}
  virtual void EndOfStream() { ++eos_signaled_; }
  virtual WriteResult WriteAsciiChunk(char* buffer, int chars_written) {
    UNREACHABLE();
  }
  virtual WriteResult WriteHeapStatsChunk(v8::HeapStatsUpdate* buffer,
                                          int updates_written) {
    ++intervals_count_;
    CHECK(updates_written);
    updates_written_ += updates_written;
    entries_count_ = 0;
    if (first_interval_index_ == -1 && updates_written != 0)
      first_interval_index_ = buffer[0].index;
    for (int i = 0; i < updates_written; ++i) {
      entries_count_ += buffer[i].count;
      entries_size_ += buffer[i].size;
    }

    return kContinue;
  }
  int eos_signaled() { return eos_signaled_; }
  int updates_written() { return updates_written_; }
  uint32_t entries_count() const { return entries_count_; }
  uint32_t entries_size() const { return entries_size_; }
  int intervals_count() const { return intervals_count_; }
  int first_interval_index() const { return first_interval_index_; }

 private:
  int eos_signaled_;
  int updates_written_;
  uint32_t entries_count_;
  uint32_t entries_size_;
  int intervals_count_;
  int first_interval_index_;
};

}  // namespace

static TestStatsStream GetHeapStatsUpdate(
    v8::HeapProfiler* heap_profiler,
    v8::SnapshotObjectId* object_id = nullptr) {
  TestStatsStream stream;
  int64_t timestamp = -1;
  v8::SnapshotObjectId last_seen_id =
      heap_profiler->GetHeapStats(&stream, &timestamp);
  if (object_id)
    *object_id = last_seen_id;
  CHECK_NE(-1, timestamp);
  CHECK_EQ(1, stream.eos_signaled());
  return stream;
}


TEST(HeapSnapshotObjectsStats) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  heap_profiler->StartTrackingHeapObjects();
  // We have to call GC 6 times. In other case the garbage will be
  // the reason of flakiness.
  for (int i = 0; i < 6; ++i) {
    CcTest::CollectAllGarbage();
  }

  v8::SnapshotObjectId initial_id;
  {
    // Single chunk of data expected in update. Initial data.
    TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler,
                                                      &initial_id);
    CHECK_EQ(1, stats_update.intervals_count());
    CHECK_EQ(1, stats_update.updates_written());
    CHECK_LT(0u, stats_update.entries_size());
    CHECK_EQ(0, stats_update.first_interval_index());
  }

  // No data expected in update because nothing has happened.
  v8::SnapshotObjectId same_id;
  CHECK_EQ(0, GetHeapStatsUpdate(heap_profiler, &same_id).updates_written());
  CHECK_EQ(initial_id, same_id);

  {
    v8::SnapshotObjectId additional_string_id;
    v8::HandleScope inner_scope_1(env->GetIsolate());
    v8_str("string1");
    {
      // Single chunk of data with one new entry expected in update.
      TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler,
                                                        &additional_string_id);
      CHECK_LT(same_id, additional_string_id);
      CHECK_EQ(1, stats_update.intervals_count());
      CHECK_EQ(1, stats_update.updates_written());
      CHECK_LT(0u, stats_update.entries_size());
      CHECK_EQ(1u, stats_update.entries_count());
      CHECK_EQ(2, stats_update.first_interval_index());
    }

    // No data expected in update because nothing happened.
    v8::SnapshotObjectId last_id;
    CHECK_EQ(0, GetHeapStatsUpdate(heap_profiler, &last_id).updates_written());
    CHECK_EQ(additional_string_id, last_id);

    {
      v8::HandleScope inner_scope_2(env->GetIsolate());
      v8_str("string2");

      uint32_t entries_size;
      {
        v8::HandleScope inner_scope_3(env->GetIsolate());
        v8_str("string3");
        v8_str("string4");

        {
          // Single chunk of data with three new entries expected in update.
          TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
          CHECK_EQ(1, stats_update.intervals_count());
          CHECK_EQ(1, stats_update.updates_written());
          CHECK_LT(0u, entries_size = stats_update.entries_size());
          CHECK_EQ(3u, stats_update.entries_count());
          CHECK_EQ(4, stats_update.first_interval_index());
        }
      }

      {
        // Single chunk of data with two left entries expected in update.
        TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
        CHECK_EQ(1, stats_update.intervals_count());
        CHECK_EQ(1, stats_update.updates_written());
        CHECK_GT(entries_size, stats_update.entries_size());
        CHECK_EQ(1u, stats_update.entries_count());
        // Two strings from forth interval were released.
        CHECK_EQ(4, stats_update.first_interval_index());
      }
    }

    {
      // Single chunk of data with 0 left entries expected in update.
      TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
      CHECK_EQ(1, stats_update.intervals_count());
      CHECK_EQ(1, stats_update.updates_written());
      CHECK_EQ(0u, stats_update.entries_size());
      CHECK_EQ(0u, stats_update.entries_count());
      // The last string from forth interval was released.
      CHECK_EQ(4, stats_update.first_interval_index());
    }
  }
  {
    // Single chunk of data with 0 left entries expected in update.
    TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
    CHECK_EQ(1, stats_update.intervals_count());
    CHECK_EQ(1, stats_update.updates_written());
    CHECK_EQ(0u, stats_update.entries_size());
    CHECK_EQ(0u, stats_update.entries_count());
    // The only string from the second interval was released.
    CHECK_EQ(2, stats_update.first_interval_index());
  }

  v8::Local<v8::Array> array = v8::Array::New(env->GetIsolate());
  CHECK_EQ(0u, array->Length());
  // Force array's buffer allocation.
  array->Set(env.local(), 2, v8_num(7)).FromJust();

  uint32_t entries_size;
  {
    // Single chunk of data with 2 entries expected in update.
    TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
    CHECK_EQ(1, stats_update.intervals_count());
    CHECK_EQ(1, stats_update.updates_written());
    CHECK_LT(0u, entries_size = stats_update.entries_size());
    // They are the array and its buffer.
    CHECK_EQ(2u, stats_update.entries_count());
    CHECK_EQ(8, stats_update.first_interval_index());
  }

  for (int i = 0; i < 100; ++i)
    array->Set(env.local(), i, v8_num(i)).FromJust();

  {
    // Single chunk of data with 1 entry expected in update.
    TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
    CHECK_EQ(1, stats_update.intervals_count());
    // The first interval was changed because old buffer was collected.
    // The second interval was changed because new buffer was allocated.
    CHECK_EQ(2, stats_update.updates_written());
    CHECK_LT(entries_size, stats_update.entries_size());
    CHECK_EQ(2u, stats_update.entries_count());
    CHECK_EQ(8, stats_update.first_interval_index());
  }

  heap_profiler->StopTrackingHeapObjects();
}


TEST(HeapObjectIds) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  const int kLength = 10;
  v8::Local<v8::Object> objects[kLength];
  v8::SnapshotObjectId ids[kLength];

  heap_profiler->StartTrackingHeapObjects(false);

  for (int i = 0; i < kLength; i++) {
    objects[i] = v8::Object::New(isolate);
  }
  GetHeapStatsUpdate(heap_profiler);

  for (int i = 0; i < kLength; i++) {
    v8::SnapshotObjectId id = heap_profiler->GetObjectId(objects[i]);
    CHECK_NE(v8::HeapProfiler::kUnknownObjectId, id);
    ids[i] = id;
  }

  heap_profiler->StopTrackingHeapObjects();
  CcTest::CollectAllAvailableGarbage();

  for (int i = 0; i < kLength; i++) {
    v8::SnapshotObjectId id = heap_profiler->GetObjectId(objects[i]);
    CHECK_EQ(ids[i], id);
    v8::Local<v8::Value> obj = heap_profiler->FindObjectById(ids[i]);
    CHECK(objects[i]->Equals(env.local(), obj).FromJust());
  }

  heap_profiler->ClearObjectIds();
  for (int i = 0; i < kLength; i++) {
    v8::SnapshotObjectId id = heap_profiler->GetObjectId(objects[i]);
    CHECK_EQ(v8::HeapProfiler::kUnknownObjectId, id);
    v8::Local<v8::Value> obj = heap_profiler->FindObjectById(ids[i]);
    CHECK(obj.IsEmpty());
  }
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
    CHECK_EQ(prop->GetToNode()->GetId(), child->GetId());
    CHECK_EQ(prop->GetToNode(), child);
    CheckChildrenIds(snapshot, child, level + 1, max_level);
  }
}


TEST(HeapSnapshotGetNodeById) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* root = snapshot->GetRoot();
  CheckChildrenIds(snapshot, root, 0, 3);
  // Check a big id, which should not exist yet.
  CHECK(!snapshot->GetNodeById(0x1000000UL));
}


TEST(HeapSnapshotGetSnapshotObjectId) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("globalObject = {};\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* global_object = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "globalObject");
  CHECK(global_object);

  v8::Local<v8::Value> globalObjectHandle =
      env->Global()->Get(env.local(), v8_str("globalObject")).ToLocalChecked();
  CHECK(!globalObjectHandle.IsEmpty());
  CHECK(globalObjectHandle->IsObject());

  v8::SnapshotObjectId id = heap_profiler->GetObjectId(globalObjectHandle);
  CHECK_NE(v8::HeapProfiler::kUnknownObjectId, id);
  CHECK_EQ(id, global_object->GetId());
}


TEST(HeapSnapshotUnknownSnapshotObjectId) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("globalObject = {};\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* node =
      snapshot->GetNodeById(v8::HeapProfiler::kUnknownObjectId);
  CHECK(!node);
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

}  // namespace


TEST(TakeHeapSnapshotAborting) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  const int snapshots_count = heap_profiler->GetSnapshotCount();
  TestActivityControl aborting_control(1);
  const v8::HeapSnapshot* no_snapshot =
      heap_profiler->TakeHeapSnapshot(&aborting_control);
  CHECK(!no_snapshot);
  CHECK_EQ(snapshots_count, heap_profiler->GetSnapshotCount());
  CHECK_GT(aborting_control.total(), aborting_control.done());

  TestActivityControl control(-1);  // Don't abort.
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot(&control);
  CHECK(ValidateSnapshot(snapshot));

  CHECK(snapshot);
  CHECK_EQ(snapshots_count + 1, heap_profiler->GetSnapshotCount());
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
    instances.push_back(this);
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
      uint16_t class_id, v8::Local<v8::Value> wrapper) {
    if (class_id == 1) {
      if (wrapper->IsString()) {
        v8::String::Utf8Value utf8(CcTest::isolate(), wrapper);
        if (strcmp(*utf8, "AAA") == 0)
          return new TestRetainedObjectInfo(1, "aaa-group", "aaa", 100);
        else if (strcmp(*utf8, "BBB") == 0)
          return new TestRetainedObjectInfo(1, "aaa-group", "aaa", 100);
      }
    } else if (class_id == 2) {
      if (wrapper->IsString()) {
        v8::String::Utf8Value utf8(CcTest::isolate(), wrapper);
        if (strcmp(*utf8, "CCC") == 0)
          return new TestRetainedObjectInfo(2, "ccc-group", "ccc");
      }
    }
    UNREACHABLE();
  }

  static std::vector<TestRetainedObjectInfo*> instances;

 private:
  bool disposed_;
  int hash_;
  const char* group_label_;
  const char* label_;
  intptr_t element_count_;
  intptr_t size_;
};

std::vector<TestRetainedObjectInfo*> TestRetainedObjectInfo::instances;

}  // namespace


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
  return nullptr;
}


TEST(HeapSnapshotRetainedObjectInfo) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();

  heap_profiler->SetWrapperClassInfoProvider(
      1, TestRetainedObjectInfo::WrapperInfoCallback);
  heap_profiler->SetWrapperClassInfoProvider(
      2, TestRetainedObjectInfo::WrapperInfoCallback);
  v8::Persistent<v8::String> p_AAA(isolate, v8_str("AAA"));
  p_AAA.SetWrapperClassId(1);
  v8::Persistent<v8::String> p_BBB(isolate, v8_str("BBB"));
  p_BBB.SetWrapperClassId(1);
  v8::Persistent<v8::String> p_CCC(isolate, v8_str("CCC"));
  p_CCC.SetWrapperClassId(2);
  CHECK(TestRetainedObjectInfo::instances.empty());
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));

  CHECK_EQ(3, TestRetainedObjectInfo::instances.size());
  for (TestRetainedObjectInfo* instance : TestRetainedObjectInfo::instances) {
    CHECK(instance->disposed());
    delete instance;
  }

  const v8::HeapGraphNode* native_group_aaa = GetNode(
      snapshot->GetRoot(), v8::HeapGraphNode::kSynthetic, "aaa-group");
  CHECK(native_group_aaa);
  CHECK_EQ(1, native_group_aaa->GetChildrenCount());
  const v8::HeapGraphNode* aaa = GetNode(
      native_group_aaa, v8::HeapGraphNode::kNative, "aaa / 100 entries");
  CHECK(aaa);
  CHECK_EQ(2, aaa->GetChildrenCount());

  const v8::HeapGraphNode* native_group_ccc = GetNode(
      snapshot->GetRoot(), v8::HeapGraphNode::kSynthetic, "ccc-group");
  const v8::HeapGraphNode* ccc = GetNode(
      native_group_ccc, v8::HeapGraphNode::kNative, "ccc");
  CHECK(ccc);

  const v8::HeapGraphNode* n_AAA = GetNode(
      aaa, v8::HeapGraphNode::kString, "AAA");
  CHECK(n_AAA);
  const v8::HeapGraphNode* n_BBB = GetNode(
      aaa, v8::HeapGraphNode::kString, "BBB");
  CHECK(n_BBB);
  CHECK_EQ(1, ccc->GetChildrenCount());
  const v8::HeapGraphNode* n_CCC = GetNode(
      ccc, v8::HeapGraphNode::kString, "CCC");
  CHECK(n_CCC);

  CHECK_EQ(aaa, GetProperty(env->GetIsolate(), n_AAA,
                            v8::HeapGraphEdge::kInternal, "native"));
  CHECK_EQ(aaa, GetProperty(env->GetIsolate(), n_BBB,
                            v8::HeapGraphEdge::kInternal, "native"));
  CHECK_EQ(ccc, GetProperty(env->GetIsolate(), n_CCC,
                            v8::HeapGraphEdge::kInternal, "native"));
}

TEST(DeleteAllHeapSnapshots) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  heap_profiler->DeleteAllHeapSnapshots();
  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  CHECK(heap_profiler->TakeHeapSnapshot());
  CHECK_EQ(1, heap_profiler->GetSnapshotCount());
  heap_profiler->DeleteAllHeapSnapshots();
  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  CHECK(heap_profiler->TakeHeapSnapshot());
  CHECK(heap_profiler->TakeHeapSnapshot());
  CHECK_EQ(2, heap_profiler->GetSnapshotCount());
  heap_profiler->DeleteAllHeapSnapshots();
  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
}


static bool FindHeapSnapshot(v8::HeapProfiler* profiler,
                             const v8::HeapSnapshot* snapshot) {
  int length = profiler->GetSnapshotCount();
  for (int i = 0; i < length; i++) {
    if (snapshot == profiler->GetHeapSnapshot(i)) return true;
  }
  return false;
}


TEST(DeleteHeapSnapshot) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  const v8::HeapSnapshot* s1 = heap_profiler->TakeHeapSnapshot();

  CHECK(s1);
  CHECK_EQ(1, heap_profiler->GetSnapshotCount());
  CHECK(FindHeapSnapshot(heap_profiler, s1));
  const_cast<v8::HeapSnapshot*>(s1)->Delete();
  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  CHECK(!FindHeapSnapshot(heap_profiler, s1));

  const v8::HeapSnapshot* s2 = heap_profiler->TakeHeapSnapshot();
  CHECK(s2);
  CHECK_EQ(1, heap_profiler->GetSnapshotCount());
  CHECK(FindHeapSnapshot(heap_profiler, s2));
  const v8::HeapSnapshot* s3 = heap_profiler->TakeHeapSnapshot();
  CHECK(s3);
  CHECK_EQ(2, heap_profiler->GetSnapshotCount());
  CHECK_NE(s2, s3);
  CHECK(FindHeapSnapshot(heap_profiler, s3));
  const_cast<v8::HeapSnapshot*>(s2)->Delete();
  CHECK_EQ(1, heap_profiler->GetSnapshotCount());
  CHECK(!FindHeapSnapshot(heap_profiler, s2));
  CHECK(FindHeapSnapshot(heap_profiler, s3));
  const_cast<v8::HeapSnapshot*>(s3)->Delete();
  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  CHECK(!FindHeapSnapshot(heap_profiler, s3));
}


class NameResolver : public v8::HeapProfiler::ObjectNameResolver {
 public:
  virtual const char* GetName(v8::Local<v8::Object> object) {
    return "Global object name";
  }
};


TEST(GlobalObjectName) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("document = { URL:\"abcdefgh\" };");

  NameResolver name_resolver;
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(nullptr, &name_resolver);
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(global);
  CHECK_EQ(0,
           strcmp("Object / Global object name",
                  const_cast<i::HeapEntry*>(
                      reinterpret_cast<const i::HeapEntry*>(global))->name()));
}


TEST(GlobalObjectFields) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("obj = {};");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* native_context =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kInternal,
                  "native_context");
  CHECK(native_context);
  const v8::HeapGraphNode* global_proxy = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kInternal, "global_proxy");
  CHECK(global_proxy);
}


TEST(NoHandleLeaks) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("document = { URL:\"abcdefgh\" };");

  i::Isolate* isolate = CcTest::i_isolate();
  int count_before = i::HandleScope::NumberOfHandles(isolate);
  heap_profiler->TakeHeapSnapshot();
  int count_after = i::HandleScope::NumberOfHandles(isolate);
  CHECK_EQ(count_before, count_after);
}


TEST(NodesIteration) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(global);
  // Verify that we can find this object by iteration.
  const int nodes_count = snapshot->GetNodesCount();
  int count = 0;
  for (int i = 0; i < nodes_count; ++i) {
    if (snapshot->GetNode(i) == global)
      ++count;
  }
  CHECK_EQ(1, count);
}


TEST(GetHeapValueForNode) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("a = { s_prop: \'value\', n_prop: \'value2\' };");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(heap_profiler->FindObjectById(global->GetId())->IsObject());
  v8::Local<v8::Object> js_global =
      env->Global()->GetPrototype().As<v8::Object>();
  CHECK(js_global == heap_profiler->FindObjectById(global->GetId()));
  const v8::HeapGraphNode* obj =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "a");
  CHECK(heap_profiler->FindObjectById(obj->GetId())->IsObject());
  v8::Local<v8::Object> js_obj = js_global->Get(env.local(), v8_str("a"))
                                     .ToLocalChecked()
                                     .As<v8::Object>();
  CHECK(js_obj == heap_profiler->FindObjectById(obj->GetId()));
  const v8::HeapGraphNode* s_prop = GetProperty(
      env->GetIsolate(), obj, v8::HeapGraphEdge::kProperty, "s_prop");
  v8::Local<v8::String> js_s_prop = js_obj->Get(env.local(), v8_str("s_prop"))
                                        .ToLocalChecked()
                                        .As<v8::String>();
  CHECK(js_s_prop == heap_profiler->FindObjectById(s_prop->GetId()));
  const v8::HeapGraphNode* n_prop = GetProperty(
      env->GetIsolate(), obj, v8::HeapGraphEdge::kProperty, "n_prop");
  v8::Local<v8::String> js_n_prop = js_obj->Get(env.local(), v8_str("n_prop"))
                                        .ToLocalChecked()
                                        .As<v8::String>();
  CHECK(js_n_prop == heap_profiler->FindObjectById(n_prop->GetId()));
}


TEST(GetHeapValueForDeletedObject) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  // It is impossible to delete a global property, so we are about to delete a
  // property of the "a" object. Also, the "p" object can't be an empty one
  // because the empty object is static and isn't actually deleted.
  CompileRun("a = { p: { r: {} } };");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* obj =
      GetProperty(env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "a");
  const v8::HeapGraphNode* prop =
      GetProperty(env->GetIsolate(), obj, v8::HeapGraphEdge::kProperty, "p");
  {
    // Perform the check inside a nested local scope to avoid creating a
    // reference to the object we are deleting.
    v8::HandleScope scope(env->GetIsolate());
    CHECK(heap_profiler->FindObjectById(prop->GetId())->IsObject());
  }
  CompileRun("delete a.p;");
  CHECK(heap_profiler->FindObjectById(prop->GetId()).IsEmpty());
}


static int StringCmp(const char* ref, i::String* act) {
  std::unique_ptr<char[]> s_act = act->ToCString();
  int result = strcmp(ref, s_act.get());
  if (result != 0)
    fprintf(stderr, "Expected: \"%s\", Actual: \"%s\"\n", ref, s_act.get());
  return result;
}


TEST(GetConstructorName) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CompileRun(
      "function Constructor1() {};\n"
      "var obj1 = new Constructor1();\n"
      "var Constructor2 = function() {};\n"
      "var obj2 = new Constructor2();\n"
      "var obj3 = {};\n"
      "obj3.__proto__ = { constructor: function Constructor3() {} };\n"
      "var obj4 = {};\n"
      "// Slow properties\n"
      "for (var i=0; i<2000; ++i) obj4[\"p\" + i] = i;\n"
      "obj4.__proto__ = { constructor: function Constructor4() {} };\n"
      "var obj5 = {};\n"
      "var obj6 = {};\n"
      "obj6.constructor = 6;");
  v8::Local<v8::Object> js_global =
      env->Global()->GetPrototype().As<v8::Object>();
  v8::Local<v8::Object> obj1 = js_global->Get(env.local(), v8_str("obj1"))
                                   .ToLocalChecked()
                                   .As<v8::Object>();
  i::Handle<i::JSObject> js_obj1 =
      i::Handle<i::JSObject>::cast(v8::Utils::OpenHandle(*obj1));
  CHECK_EQ(0, StringCmp(
      "Constructor1", i::V8HeapExplorer::GetConstructorName(*js_obj1)));
  v8::Local<v8::Object> obj2 = js_global->Get(env.local(), v8_str("obj2"))
                                   .ToLocalChecked()
                                   .As<v8::Object>();
  i::Handle<i::JSObject> js_obj2 =
      i::Handle<i::JSObject>::cast(v8::Utils::OpenHandle(*obj2));
  CHECK_EQ(0, StringCmp(
      "Constructor2", i::V8HeapExplorer::GetConstructorName(*js_obj2)));
  v8::Local<v8::Object> obj3 = js_global->Get(env.local(), v8_str("obj3"))
                                   .ToLocalChecked()
                                   .As<v8::Object>();
  i::Handle<i::JSObject> js_obj3 =
      i::Handle<i::JSObject>::cast(v8::Utils::OpenHandle(*obj3));
  CHECK_EQ(0, StringCmp("Constructor3",
                        i::V8HeapExplorer::GetConstructorName(*js_obj3)));
  v8::Local<v8::Object> obj4 = js_global->Get(env.local(), v8_str("obj4"))
                                   .ToLocalChecked()
                                   .As<v8::Object>();
  i::Handle<i::JSObject> js_obj4 =
      i::Handle<i::JSObject>::cast(v8::Utils::OpenHandle(*obj4));
  CHECK_EQ(0, StringCmp("Constructor4",
                        i::V8HeapExplorer::GetConstructorName(*js_obj4)));
  v8::Local<v8::Object> obj5 = js_global->Get(env.local(), v8_str("obj5"))
                                   .ToLocalChecked()
                                   .As<v8::Object>();
  i::Handle<i::JSObject> js_obj5 =
      i::Handle<i::JSObject>::cast(v8::Utils::OpenHandle(*obj5));
  CHECK_EQ(0, StringCmp(
      "Object", i::V8HeapExplorer::GetConstructorName(*js_obj5)));
  v8::Local<v8::Object> obj6 = js_global->Get(env.local(), v8_str("obj6"))
                                   .ToLocalChecked()
                                   .As<v8::Object>();
  i::Handle<i::JSObject> js_obj6 =
      i::Handle<i::JSObject>::cast(v8::Utils::OpenHandle(*obj6));
  CHECK_EQ(0, StringCmp(
      "Object", i::V8HeapExplorer::GetConstructorName(*js_obj6)));
}


TEST(FastCaseAccessors) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("var obj1 = {};\n"
             "obj1.__defineGetter__('propWithGetter', function Y() {\n"
             "  return 42;\n"
             "});\n"
             "obj1.__defineSetter__('propWithSetter', function Z(value) {\n"
             "  return this.value_ = value;\n"
             "});\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(global);
  const v8::HeapGraphNode* obj1 = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "obj1");
  CHECK(obj1);
  const v8::HeapGraphNode* func;
  func = GetProperty(env->GetIsolate(), obj1, v8::HeapGraphEdge::kProperty,
                     "get propWithGetter");
  CHECK(func);
  func = GetProperty(env->GetIsolate(), obj1, v8::HeapGraphEdge::kProperty,
                     "set propWithGetter");
  CHECK(!func);
  func = GetProperty(env->GetIsolate(), obj1, v8::HeapGraphEdge::kProperty,
                     "set propWithSetter");
  CHECK(func);
  func = GetProperty(env->GetIsolate(), obj1, v8::HeapGraphEdge::kProperty,
                     "get propWithSetter");
  CHECK(!func);
}


TEST(FastCaseRedefinedAccessors) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "var obj1 = {};\n"
      "Object.defineProperty(obj1, 'prop', { "
      "  get: function() { return 42; },\n"
      "  set: function(value) { return this.prop_ = value; },\n"
      "  configurable: true,\n"
      "  enumerable: true,\n"
      "});\n"
      "Object.defineProperty(obj1, 'prop', { "
      "  get: function() { return 153; },\n"
      "  set: function(value) { return this.prop_ = value; },\n"
      "  configurable: true,\n"
      "  enumerable: true,\n"
      "});\n");
  v8::Local<v8::Object> js_global =
      env->Global()->GetPrototype().As<v8::Object>();
  i::Handle<i::JSReceiver> js_obj1 =
      v8::Utils::OpenHandle(*js_global->Get(env.local(), v8_str("obj1"))
                                 .ToLocalChecked()
                                 .As<v8::Object>());
  USE(js_obj1);

  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(global);
  const v8::HeapGraphNode* obj1 = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "obj1");
  CHECK(obj1);
  const v8::HeapGraphNode* func;
  func = GetProperty(env->GetIsolate(), obj1, v8::HeapGraphEdge::kProperty,
                     "get prop");
  CHECK(func);
  func = GetProperty(env->GetIsolate(), obj1, v8::HeapGraphEdge::kProperty,
                     "set prop");
  CHECK(func);
}


TEST(SlowCaseAccessors) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("var obj1 = {};\n"
             "for (var i = 0; i < 100; ++i) obj1['z' + i] = {};"
             "obj1.__defineGetter__('propWithGetter', function Y() {\n"
             "  return 42;\n"
             "});\n"
             "obj1.__defineSetter__('propWithSetter', function Z(value) {\n"
             "  return this.value_ = value;\n"
             "});\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(global);
  const v8::HeapGraphNode* obj1 = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "obj1");
  CHECK(obj1);
  const v8::HeapGraphNode* func;
  func = GetProperty(env->GetIsolate(), obj1, v8::HeapGraphEdge::kProperty,
                     "get propWithGetter");
  CHECK(func);
  func = GetProperty(env->GetIsolate(), obj1, v8::HeapGraphEdge::kProperty,
                     "set propWithGetter");
  CHECK(!func);
  func = GetProperty(env->GetIsolate(), obj1, v8::HeapGraphEdge::kProperty,
                     "set propWithSetter");
  CHECK(func);
  func = GetProperty(env->GetIsolate(), obj1, v8::HeapGraphEdge::kProperty,
                     "get propWithSetter");
  CHECK(!func);
}


TEST(HiddenPropertiesFastCase) {
  v8::Isolate* isolate = CcTest::isolate();
  LocalContext env;
  v8::HandleScope scope(isolate);
  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();

  CompileRun(
      "function C(x) { this.a = this; this.b = x; }\n"
      "c = new C(2012);\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* c =
      GetProperty(isolate, global, v8::HeapGraphEdge::kProperty, "c");
  CHECK(c);
  const v8::HeapGraphNode* hidden_props =
      GetProperty(isolate, global, v8::HeapGraphEdge::kProperty, "<symbol>");
  CHECK(!hidden_props);

  v8::Local<v8::Value> cHandle =
      env->Global()->Get(env.local(), v8_str("c")).ToLocalChecked();
  CHECK(!cHandle.IsEmpty() && cHandle->IsObject());
  cHandle->ToObject(env.local())
      .ToLocalChecked()
      ->SetPrivate(env.local(),
                   v8::Private::ForApi(env->GetIsolate(), v8_str("key")),
                   v8_str("val"))
      .FromJust();

  snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  global = GetGlobalObject(snapshot);
  c = GetProperty(isolate, global, v8::HeapGraphEdge::kProperty, "c");
  CHECK(c);
  hidden_props =
      GetProperty(isolate, c, v8::HeapGraphEdge::kProperty, "<symbol>");
  CHECK(hidden_props);
}


TEST(AccessorInfo) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("function foo(x) { }\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* foo = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "foo");
  CHECK(foo);
  const v8::HeapGraphNode* map =
      GetProperty(env->GetIsolate(), foo, v8::HeapGraphEdge::kInternal, "map");
  CHECK(map);
  const v8::HeapGraphNode* descriptors = GetProperty(
      env->GetIsolate(), map, v8::HeapGraphEdge::kInternal, "descriptors");
  CHECK(descriptors);
  const v8::HeapGraphNode* length_name = GetProperty(
      env->GetIsolate(), descriptors, v8::HeapGraphEdge::kInternal, "2");
  CHECK(length_name);
  CHECK_EQ(0, strcmp("length", *v8::String::Utf8Value(env->GetIsolate(),
                                                      length_name->GetName())));
  const v8::HeapGraphNode* length_accessor = GetProperty(
      env->GetIsolate(), descriptors, v8::HeapGraphEdge::kInternal, "4");
  CHECK(length_accessor);
  CHECK_EQ(0, strcmp("system / AccessorInfo",
                     *v8::String::Utf8Value(env->GetIsolate(),
                                            length_accessor->GetName())));
  const v8::HeapGraphNode* name = GetProperty(
      env->GetIsolate(), length_accessor, v8::HeapGraphEdge::kInternal, "name");
  CHECK(name);
  const v8::HeapGraphNode* getter =
      GetProperty(env->GetIsolate(), length_accessor,
                  v8::HeapGraphEdge::kInternal, "getter");
  CHECK(getter);
  const v8::HeapGraphNode* setter =
      GetProperty(env->GetIsolate(), length_accessor,
                  v8::HeapGraphEdge::kInternal, "setter");
  CHECK(setter);
}


bool HasWeakEdge(const v8::HeapGraphNode* node) {
  for (int i = 0; i < node->GetChildrenCount(); ++i) {
    const v8::HeapGraphEdge* handle_edge = node->GetChild(i);
    if (handle_edge->GetType() == v8::HeapGraphEdge::kWeak) return true;
  }
  return false;
}


bool HasWeakGlobalHandle() {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* gc_roots = GetNode(
      snapshot->GetRoot(), v8::HeapGraphNode::kSynthetic, "(GC roots)");
  CHECK(gc_roots);
  const v8::HeapGraphNode* global_handles = GetNode(
      gc_roots, v8::HeapGraphNode::kSynthetic, "(Global handles)");
  CHECK(global_handles);
  return HasWeakEdge(global_handles);
}


static void PersistentHandleCallback(
    const v8::WeakCallbackInfo<v8::Persistent<v8::Object> >& data) {
  data.GetParameter()->Reset();
}


TEST(WeakGlobalHandle) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CHECK(!HasWeakGlobalHandle());

  v8::Persistent<v8::Object> handle;

  handle.Reset(env->GetIsolate(), v8::Object::New(env->GetIsolate()));
  handle.SetWeak(&handle, PersistentHandleCallback,
                 v8::WeakCallbackType::kParameter);

  CHECK(HasWeakGlobalHandle());
  CcTest::CollectAllGarbage();
  EmptyMessageQueues(env->GetIsolate());
}


TEST(SfiAndJsFunctionWeakRefs) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "fun = (function (x) { return function () { return x + 1; } })(1);");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(global);
  const v8::HeapGraphNode* fun = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "fun");
  CHECK(!HasWeakEdge(fun));
  const v8::HeapGraphNode* shared = GetProperty(
      env->GetIsolate(), fun, v8::HeapGraphEdge::kInternal, "shared");
  CHECK(!HasWeakEdge(shared));
}


TEST(NoDebugObjectInSnapshot) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CHECK(CcTest::i_isolate()->debug()->Load());
  CompileRun("foo = {};");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* root = snapshot->GetRoot();
  int globals_count = 0;
  for (int i = 0; i < root->GetChildrenCount(); ++i) {
    const v8::HeapGraphEdge* edge = root->GetChild(i);
    if (edge->GetType() == v8::HeapGraphEdge::kShortcut) {
      ++globals_count;
      const v8::HeapGraphNode* global = edge->GetToNode();
      const v8::HeapGraphNode* foo = GetProperty(
          env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "foo");
      CHECK(foo);
    }
  }
  CHECK_EQ(1, globals_count);
}


TEST(AllStrongGcRootsHaveNames) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("foo = {};");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* gc_roots = GetNode(
      snapshot->GetRoot(), v8::HeapGraphNode::kSynthetic, "(GC roots)");
  CHECK(gc_roots);
  const v8::HeapGraphNode* strong_roots = GetNode(
      gc_roots, v8::HeapGraphNode::kSynthetic, "(Strong roots)");
  CHECK(strong_roots);
  for (int i = 0; i < strong_roots->GetChildrenCount(); ++i) {
    const v8::HeapGraphEdge* edge = strong_roots->GetChild(i);
    CHECK_EQ(v8::HeapGraphEdge::kInternal, edge->GetType());
    v8::String::Utf8Value name(env->GetIsolate(), edge->GetName());
    CHECK(isalpha(**name));
  }
}


TEST(NoRefsToNonEssentialEntries) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("global_object = {};\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* global_object = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "global_object");
  CHECK(global_object);
  const v8::HeapGraphNode* properties =
      GetProperty(env->GetIsolate(), global_object,
                  v8::HeapGraphEdge::kInternal, "properties");
  CHECK(!properties);
  const v8::HeapGraphNode* elements =
      GetProperty(env->GetIsolate(), global_object,
                  v8::HeapGraphEdge::kInternal, "elements");
  CHECK(!elements);
}


TEST(MapHasDescriptorsAndTransitions) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("obj = { a: 10 };\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* global_object = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "obj");
  CHECK(global_object);

  const v8::HeapGraphNode* map = GetProperty(
      env->GetIsolate(), global_object, v8::HeapGraphEdge::kInternal, "map");
  CHECK(map);
  const v8::HeapGraphNode* own_descriptors = GetProperty(
      env->GetIsolate(), map, v8::HeapGraphEdge::kInternal, "descriptors");
  CHECK(own_descriptors);
  const v8::HeapGraphNode* own_transitions = GetProperty(
      env->GetIsolate(), map, v8::HeapGraphEdge::kInternal, "transitions");
  CHECK(!own_transitions);
}


TEST(ManyLocalsInSharedContext) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  int num_objects = 6000;
  CompileRun(
      "var n = 6000;"
      "var result = [];"
      "result.push('(function outer() {');"
      "for (var i = 0; i < n; i++) {"
      "    var f = 'function f_' + i + '() { ';"
      "    if (i > 0)"
      "        f += 'f_' + (i - 1) + '();';"
      "    f += ' }';"
      "    result.push(f);"
      "}"
      "result.push('return f_' + (n - 1) + ';');"
      "result.push('})()');"
      "var ok = eval(result.join('\\n'));");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(global);
  const v8::HeapGraphNode* ok_object = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "ok");
  CHECK(ok_object);
  const v8::HeapGraphNode* context_object = GetProperty(
      env->GetIsolate(), ok_object, v8::HeapGraphEdge::kInternal, "context");
  CHECK(context_object);
  // Check the objects are not duplicated in the context.
  CHECK_EQ(v8::internal::Context::MIN_CONTEXT_SLOTS + num_objects - 1,
           context_object->GetChildrenCount());
  // Check all the objects have got their names.
  // ... well check just every 15th because otherwise it's too slow in debug.
  for (int i = 0; i < num_objects - 1; i += 15) {
    i::EmbeddedVector<char, 100> var_name;
    i::SNPrintF(var_name, "f_%d", i);
    const v8::HeapGraphNode* f_object =
        GetProperty(env->GetIsolate(), context_object,
                    v8::HeapGraphEdge::kContextVariable, var_name.start());
    CHECK(f_object);
  }
}


TEST(AllocationSitesAreVisible) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();
  CompileRun(
      "fun = function () { var a = [3, 2, 1]; return a; }\n"
      "fun();");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(global);
  const v8::HeapGraphNode* fun_code = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "fun");
  CHECK(fun_code);
  const v8::HeapGraphNode* vector_cell =
      GetProperty(env->GetIsolate(), fun_code, v8::HeapGraphEdge::kInternal,
                  "feedback_vector_cell");
  // TODO(mvstanton): I'm not sure if this is the best way to expose
  // literals. Is it too much to expose the Cell?
  CHECK(vector_cell);
  const v8::HeapGraphNode* vector = GetProperty(
      env->GetIsolate(), vector_cell, v8::HeapGraphEdge::kInternal, "value");
  CHECK_EQ(v8::HeapGraphNode::kArray, vector->GetType());
  CHECK_EQ(3, vector->GetChildrenCount());

  // The first value in the feedback vector should be the boilerplate,
  // after an AllocationSite.
  const v8::HeapGraphEdge* prop = vector->GetChild(2);
  const v8::HeapGraphNode* allocation_site = prop->GetToNode();
  v8::String::Utf8Value name(env->GetIsolate(), allocation_site->GetName());
  CHECK_EQ(0, strcmp("system / AllocationSite", *name));
  const v8::HeapGraphNode* transition_info =
      GetProperty(env->GetIsolate(), allocation_site,
                  v8::HeapGraphEdge::kInternal, "transition_info");
  CHECK(transition_info);

  const v8::HeapGraphNode* elements =
      GetProperty(env->GetIsolate(), transition_info,
                  v8::HeapGraphEdge::kInternal, "elements");
  CHECK(elements);
  CHECK_EQ(v8::HeapGraphNode::kArray, elements->GetType());
  CHECK_EQ(v8::internal::FixedArray::SizeFor(3),
           static_cast<int>(elements->GetShallowSize()));

  v8::Local<v8::Value> array_val =
      heap_profiler->FindObjectById(transition_info->GetId());
  CHECK(array_val->IsArray());
  v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(array_val);
  // Verify the array is "a" in the code above.
  CHECK_EQ(3u, array->Length());
  CHECK(v8::Integer::New(isolate, 3)
            ->Equals(env.local(),
                     array->Get(env.local(), v8::Integer::New(isolate, 0))
                         .ToLocalChecked())
            .FromJust());
  CHECK(v8::Integer::New(isolate, 2)
            ->Equals(env.local(),
                     array->Get(env.local(), v8::Integer::New(isolate, 1))
                         .ToLocalChecked())
            .FromJust());
  CHECK(v8::Integer::New(isolate, 1)
            ->Equals(env.local(),
                     array->Get(env.local(), v8::Integer::New(isolate, 2))
                         .ToLocalChecked())
            .FromJust());
}


TEST(JSFunctionHasCodeLink) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("function foo(x, y) { return x + y; }\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* foo_func = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "foo");
  CHECK(foo_func);
  const v8::HeapGraphNode* code = GetProperty(
      env->GetIsolate(), foo_func, v8::HeapGraphEdge::kInternal, "code");
  CHECK(code);
}

static const v8::HeapGraphNode* GetNodeByPath(v8::Isolate* isolate,
                                              const v8::HeapSnapshot* snapshot,
                                              const char* path[], int depth) {
  const v8::HeapGraphNode* node = snapshot->GetRoot();
  for (int current_depth = 0; current_depth < depth; ++current_depth) {
    int i, count = node->GetChildrenCount();
    for (i = 0; i < count; ++i) {
      const v8::HeapGraphEdge* edge = node->GetChild(i);
      const v8::HeapGraphNode* to_node = edge->GetToNode();
      v8::String::Utf8Value edge_name(isolate, edge->GetName());
      v8::String::Utf8Value node_name(isolate, to_node->GetName());
      i::EmbeddedVector<char, 100> name;
      i::SNPrintF(name, "%s::%s", *edge_name, *node_name);
      if (strstr(name.start(), path[current_depth])) {
        node = to_node;
        break;
      }
    }
    if (i == count) return nullptr;
  }
  return node;
}


TEST(CheckCodeNames) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("var a = 1.1;");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));

  const char* stub_path[] = {
    "::(GC roots)",
    "::(Strong roots)",
    "code_stubs::",
    "::(ArraySingleArgumentConstructorStub code)"
  };
  const v8::HeapGraphNode* node = GetNodeByPath(
      env->GetIsolate(), snapshot, stub_path, arraysize(stub_path));
  CHECK(node);

  const char* builtin_path1[] = {"::(GC roots)", "::(Builtins)",
                                 "::(KeyedLoadIC_Slow builtin)"};
  node = GetNodeByPath(env->GetIsolate(), snapshot, builtin_path1,
                       arraysize(builtin_path1));
  CHECK(node);

  const char* builtin_path2[] = {"::(GC roots)", "::(Builtins)",
                                 "::(CompileLazy builtin)"};
  node = GetNodeByPath(env->GetIsolate(), snapshot, builtin_path2,
                       arraysize(builtin_path2));
  CHECK(node);
  v8::String::Utf8Value node_name(env->GetIsolate(), node->GetName());
  CHECK_EQ(0, strcmp("(CompileLazy builtin)", *node_name));
}


static const char* record_trace_tree_source =
"var topFunctions = [];\n"
"var global = this;\n"
"function generateFunctions(width, depth) {\n"
"  var script = [];\n"
"  for (var i = 0; i < width; i++) {\n"
"    for (var j = 0; j < depth; j++) {\n"
"      script.push('function f_' + i + '_' + j + '(x) {\\n');\n"
"      script.push('  try {\\n');\n"
"      if (j < depth-2) {\n"
"        script.push('    return f_' + i + '_' + (j+1) + '(x+1);\\n');\n"
"      } else if (j == depth - 2) {\n"
"        script.push('    return new f_' + i + '_' + (depth - 1) + '();\\n');\n"
"      } else if (j == depth - 1) {\n"
"        script.push('    this.ts = Date.now();\\n');\n"
"      }\n"
"      script.push('  } catch (e) {}\\n');\n"
"      script.push('}\\n');\n"
"      \n"
"    }\n"
"  }\n"
"  var script = script.join('');\n"
"  // throw script;\n"
"  global.eval(script);\n"
"  for (var i = 0; i < width; i++) {\n"
"    topFunctions.push(this['f_' + i + '_0']);\n"
"  }\n"
"}\n"
"\n"
"var width = 3;\n"
"var depth = 3;\n"
"generateFunctions(width, depth);\n"
"var instances = [];\n"
"function start() {\n"
"  for (var i = 0; i < width; i++) {\n"
"    instances.push(topFunctions[i](0));\n"
"  }\n"
"}\n"
"\n"
"for (var i = 0; i < 100; i++) start();\n";


static AllocationTraceNode* FindNode(
    AllocationTracker* tracker, const Vector<const char*>& names) {
  AllocationTraceNode* node = tracker->trace_tree()->root();
  for (int i = 0; node != nullptr && i < names.length(); i++) {
    const char* name = names[i];
    const std::vector<AllocationTraceNode*>& children = node->children();
    node = nullptr;
    for (AllocationTraceNode* child : children) {
      unsigned index = child->function_info_index();
      AllocationTracker::FunctionInfo* info =
          tracker->function_info_list()[index];
      if (info && strcmp(info->name, name) == 0) {
        node = child;
        break;
      }
    }
  }
  return node;
}


TEST(ArrayGrowLeftTrim) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  heap_profiler->StartTrackingHeapObjects(true);

  CompileRun(
    "var a = [];\n"
    "for (var i = 0; i < 5; ++i)\n"
    "    a[i] = i;\n"
    "for (var i = 0; i < 3; ++i)\n"
    "    a.shift();\n");

  const char* names[] = {""};
  AllocationTracker* tracker =
      reinterpret_cast<i::HeapProfiler*>(heap_profiler)->allocation_tracker();
  CHECK(tracker);
  // Resolve all function locations.
  tracker->PrepareForSerialization();
  // Print for better diagnostics in case of failure.
  tracker->trace_tree()->Print(tracker);

  AllocationTraceNode* node = FindNode(tracker, ArrayVector(names));
  CHECK(node);
  CHECK_GE(node->allocation_count(), 2u);
  CHECK_GE(node->allocation_size(), 4u * 5u);
  heap_profiler->StopTrackingHeapObjects();
}

TEST(TrackHeapAllocationsWithInlining) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;

  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  heap_profiler->StartTrackingHeapObjects(true);

  CompileRun(record_trace_tree_source);

  AllocationTracker* tracker =
      reinterpret_cast<i::HeapProfiler*>(heap_profiler)->allocation_tracker();
  CHECK(tracker);
  // Resolve all function locations.
  tracker->PrepareForSerialization();
  // Print for better diagnostics in case of failure.
  tracker->trace_tree()->Print(tracker);

  const char* names[] = {"", "start", "f_0_0"};
  AllocationTraceNode* node = FindNode(tracker, ArrayVector(names));
  CHECK(node);
  CHECK_GE(node->allocation_count(), 10u);
  CHECK_GE(node->allocation_size(), 4 * node->allocation_count());
  heap_profiler->StopTrackingHeapObjects();
}

TEST(TrackHeapAllocationsWithoutInlining) {
  i::FLAG_turbo_inlining = false;
  // Disable inlining
  i::FLAG_max_inlined_bytecode_size = 0;
  i::FLAG_max_inlined_bytecode_size_small = 0;
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;

  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  heap_profiler->StartTrackingHeapObjects(true);

  CompileRun(record_trace_tree_source);

  AllocationTracker* tracker =
      reinterpret_cast<i::HeapProfiler*>(heap_profiler)->allocation_tracker();
  CHECK(tracker);
  // Resolve all function locations.
  tracker->PrepareForSerialization();
  // Print for better diagnostics in case of failure.
  tracker->trace_tree()->Print(tracker);

  const char* names[] = {"", "start", "f_0_0", "f_0_1", "f_0_2"};
  AllocationTraceNode* node = FindNode(tracker, ArrayVector(names));
  CHECK(node);
  CHECK_GE(node->allocation_count(), 100u);
  CHECK_GE(node->allocation_size(), 4 * node->allocation_count());
  heap_profiler->StopTrackingHeapObjects();
}


static const char* inline_heap_allocation_source =
    "function f_0(x) {\n"
    "  return f_1(x+1);\n"
    "}\n"
    "%NeverOptimizeFunction(f_0);\n"
    "function f_1(x) {\n"
    "  return new f_2(x+1);\n"
    "}\n"
    "%NeverOptimizeFunction(f_1);\n"
    "function f_2(x) {\n"
    "  this.foo = x;\n"
    "}\n"
    "var instances = [];\n"
    "function start() {\n"
    "  instances.push(f_0(0));\n"
    "}\n"
    "\n"
    "for (var i = 0; i < 100; i++) start();\n";


TEST(TrackBumpPointerAllocations) {
  i::FLAG_allow_natives_syntax = true;
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;

  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  const char* names[] = {"", "start", "f_0", "f_1"};
  // First check that normally all allocations are recorded.
  {
    heap_profiler->StartTrackingHeapObjects(true);

    CompileRun(inline_heap_allocation_source);

    AllocationTracker* tracker =
        reinterpret_cast<i::HeapProfiler*>(heap_profiler)->allocation_tracker();
    CHECK(tracker);
    // Resolve all function locations.
    tracker->PrepareForSerialization();
    // Print for better diagnostics in case of failure.
    tracker->trace_tree()->Print(tracker);

    AllocationTraceNode* node = FindNode(tracker, ArrayVector(names));
    CHECK(node);
    CHECK_GE(node->allocation_count(), 100u);
    CHECK_GE(node->allocation_size(), 4 * node->allocation_count());
    heap_profiler->StopTrackingHeapObjects();
  }

  {
    heap_profiler->StartTrackingHeapObjects(true);

    // Now check that not all allocations are tracked if we manually reenable
    // inline allocations.
    CHECK(CcTest::heap()->inline_allocation_disabled());
    CcTest::heap()->EnableInlineAllocation();

    CompileRun(inline_heap_allocation_source);

    AllocationTracker* tracker =
        reinterpret_cast<i::HeapProfiler*>(heap_profiler)->allocation_tracker();
    CHECK(tracker);
    // Resolve all function locations.
    tracker->PrepareForSerialization();
    // Print for better diagnostics in case of failure.
    tracker->trace_tree()->Print(tracker);

    AllocationTraceNode* node = FindNode(tracker, ArrayVector(names));
    CHECK(node);
    CHECK_LT(node->allocation_count(), 100u);

    CcTest::heap()->DisableInlineAllocation();
    heap_profiler->StopTrackingHeapObjects();
  }
}


TEST(TrackV8ApiAllocation) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;

  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  const char* names[] = { "(V8 API)" };
  heap_profiler->StartTrackingHeapObjects(true);

  v8::Local<v8::Object> o1 = v8::Object::New(env->GetIsolate());
  o1->Clone();

  AllocationTracker* tracker =
      reinterpret_cast<i::HeapProfiler*>(heap_profiler)->allocation_tracker();
  CHECK(tracker);
  // Resolve all function locations.
  tracker->PrepareForSerialization();
  // Print for better diagnostics in case of failure.
  tracker->trace_tree()->Print(tracker);

  AllocationTraceNode* node = FindNode(tracker, ArrayVector(names));
  CHECK(node);
  CHECK_GE(node->allocation_count(), 2u);
  CHECK_GE(node->allocation_size(), 4 * node->allocation_count());
  heap_profiler->StopTrackingHeapObjects();
}


TEST(ArrayBufferAndArrayBufferView) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("arr1 = new Uint32Array(100);\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* arr1_obj = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "arr1");
  CHECK(arr1_obj);
  const v8::HeapGraphNode* arr1_buffer = GetProperty(
      env->GetIsolate(), arr1_obj, v8::HeapGraphEdge::kInternal, "buffer");
  CHECK(arr1_buffer);
  const v8::HeapGraphNode* backing_store =
      GetProperty(env->GetIsolate(), arr1_buffer, v8::HeapGraphEdge::kInternal,
                  "backing_store");
  CHECK(backing_store);
  CHECK_EQ(400, static_cast<int>(backing_store->GetShallowSize()));
}


static int GetRetainersCount(const v8::HeapSnapshot* snapshot,
                             const v8::HeapGraphNode* node) {
  int count = 0;
  for (int i = 0, l = snapshot->GetNodesCount(); i < l; ++i) {
    const v8::HeapGraphNode* parent = snapshot->GetNode(i);
    for (int j = 0, l2 = parent->GetChildrenCount(); j < l2; ++j) {
      if (parent->GetChild(j)->GetToNode() == node) {
        ++count;
      }
    }
  }
  return count;
}


TEST(ArrayBufferSharedBackingStore) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();

  v8::Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 1024);
  CHECK_EQ(1024, static_cast<int>(ab->ByteLength()));
  CHECK(!ab->IsExternal());
  v8::ArrayBuffer::Contents ab_contents = ab->Externalize();
  CHECK(ab->IsExternal());

  CHECK_EQ(1024, static_cast<int>(ab_contents.ByteLength()));
  void* data = ab_contents.Data();
  CHECK_NOT_NULL(data);
  v8::Local<v8::ArrayBuffer> ab2 =
      v8::ArrayBuffer::New(isolate, data, ab_contents.ByteLength());
  CHECK(ab2->IsExternal());
  env->Global()->Set(env.local(), v8_str("ab1"), ab).FromJust();
  env->Global()->Set(env.local(), v8_str("ab2"), ab2).FromJust();

  v8::Local<v8::Value> result = CompileRun("ab2.byteLength");
  CHECK_EQ(1024, result->Int32Value(env.local()).FromJust());

  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* ab1_node = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "ab1");
  CHECK(ab1_node);
  const v8::HeapGraphNode* ab1_data =
      GetProperty(env->GetIsolate(), ab1_node, v8::HeapGraphEdge::kInternal,
                  "backing_store");
  CHECK(ab1_data);
  const v8::HeapGraphNode* ab2_node = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "ab2");
  CHECK(ab2_node);
  const v8::HeapGraphNode* ab2_data =
      GetProperty(env->GetIsolate(), ab2_node, v8::HeapGraphEdge::kInternal,
                  "backing_store");
  CHECK(ab2_data);
  CHECK_EQ(ab1_data, ab2_data);
  CHECK_EQ(2, GetRetainersCount(snapshot, ab1_data));
  free(data);
}


TEST(WeakContainers) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  if (!CcTest::i_isolate()->use_optimizer()) return;
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun(
      "function foo(a) { return a.x; }\n"
      "obj = {x : 123};\n"
      "foo(obj);\n"
      "foo(obj);\n"
      "%OptimizeFunctionOnNextCall(foo);\n"
      "foo(obj);\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* obj = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "obj");
  CHECK(obj);
  const v8::HeapGraphNode* map =
      GetProperty(env->GetIsolate(), obj, v8::HeapGraphEdge::kInternal, "map");
  CHECK(map);
  const v8::HeapGraphNode* dependent_code = GetProperty(
      env->GetIsolate(), map, v8::HeapGraphEdge::kInternal, "dependent_code");
  if (!dependent_code) return;
  int count = dependent_code->GetChildrenCount();
  CHECK_NE(0, count);
  for (int i = 0; i < count; ++i) {
    const v8::HeapGraphEdge* prop = dependent_code->GetChild(i);
    CHECK_EQ(v8::HeapGraphEdge::kInternal, prop->GetType());
  }
}

TEST(JSPromise) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun(
      "function A() {}\n"
      "function B() {}\n"
      "resolved = Promise.resolve(new A());\n"
      "rejected = Promise.reject(new B());\n"
      "pending = new Promise(() => 0);\n"
      "chained = pending.then(A, B);\n");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);

  const v8::HeapGraphNode* resolved = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "resolved");
  CHECK(GetProperty(env->GetIsolate(), resolved, v8::HeapGraphEdge::kInternal,
                    "result"));

  const v8::HeapGraphNode* rejected = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "rejected");
  CHECK(GetProperty(env->GetIsolate(), rejected, v8::HeapGraphEdge::kInternal,
                    "result"));

  const v8::HeapGraphNode* pending = GetProperty(
      env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, "pending");
  CHECK(GetProperty(env->GetIsolate(), pending, v8::HeapGraphEdge::kInternal,
                    "deferred_promise"));
  CHECK(GetProperty(env->GetIsolate(), pending, v8::HeapGraphEdge::kInternal,
                    "fulfill_reactions"));
  CHECK(GetProperty(env->GetIsolate(), pending, v8::HeapGraphEdge::kInternal,
                    "reject_reactions"));

  const char* objectNames[] = {"resolved", "rejected", "pending", "chained"};
  for (auto objectName : objectNames) {
    const v8::HeapGraphNode* promise = GetProperty(
        env->GetIsolate(), global, v8::HeapGraphEdge::kProperty, objectName);
    EnsureNoUninstrumentedInternals(env->GetIsolate(), promise);
  }
}

static inline i::Address ToAddress(int n) {
  return reinterpret_cast<i::Address>(n);
}


TEST(AddressToTraceMap) {
  i::AddressToTraceMap map;

  CHECK_EQ(0u, map.GetTraceNodeId(ToAddress(150)));

  // [0x100, 0x200) -> 1
  map.AddRange(ToAddress(0x100), 0x100, 1U);
  CHECK_EQ(0u, map.GetTraceNodeId(ToAddress(0x50)));
  CHECK_EQ(1u, map.GetTraceNodeId(ToAddress(0x100)));
  CHECK_EQ(1u, map.GetTraceNodeId(ToAddress(0x150)));
  CHECK_EQ(0u, map.GetTraceNodeId(ToAddress(0x100 + 0x100)));
  CHECK_EQ(1u, map.size());

  // [0x100, 0x200) -> 1, [0x200, 0x300) -> 2
  map.AddRange(ToAddress(0x200), 0x100, 2U);
  CHECK_EQ(2u, map.GetTraceNodeId(ToAddress(0x2A0)));
  CHECK_EQ(2u, map.size());

  // [0x100, 0x180) -> 1, [0x180, 0x280) -> 3, [0x280, 0x300) -> 2
  map.AddRange(ToAddress(0x180), 0x100, 3U);
  CHECK_EQ(1u, map.GetTraceNodeId(ToAddress(0x17F)));
  CHECK_EQ(2u, map.GetTraceNodeId(ToAddress(0x280)));
  CHECK_EQ(3u, map.GetTraceNodeId(ToAddress(0x180)));
  CHECK_EQ(3u, map.size());

  // [0x100, 0x180) -> 1, [0x180, 0x280) -> 3, [0x280, 0x300) -> 2,
  // [0x400, 0x500) -> 4
  map.AddRange(ToAddress(0x400), 0x100, 4U);
  CHECK_EQ(1u, map.GetTraceNodeId(ToAddress(0x17F)));
  CHECK_EQ(2u, map.GetTraceNodeId(ToAddress(0x280)));
  CHECK_EQ(3u, map.GetTraceNodeId(ToAddress(0x180)));
  CHECK_EQ(4u, map.GetTraceNodeId(ToAddress(0x450)));
  CHECK_EQ(0u, map.GetTraceNodeId(ToAddress(0x500)));
  CHECK_EQ(0u, map.GetTraceNodeId(ToAddress(0x350)));
  CHECK_EQ(4u, map.size());

  // [0x100, 0x180) -> 1, [0x180, 0x200) -> 3, [0x200, 0x600) -> 5
  map.AddRange(ToAddress(0x200), 0x400, 5U);
  CHECK_EQ(5u, map.GetTraceNodeId(ToAddress(0x200)));
  CHECK_EQ(5u, map.GetTraceNodeId(ToAddress(0x400)));
  CHECK_EQ(3u, map.size());

  // [0x100, 0x180) -> 1, [0x180, 0x200) -> 7, [0x200, 0x600) ->5
  map.AddRange(ToAddress(0x180), 0x80, 6U);
  map.AddRange(ToAddress(0x180), 0x80, 7U);
  CHECK_EQ(7u, map.GetTraceNodeId(ToAddress(0x180)));
  CHECK_EQ(5u, map.GetTraceNodeId(ToAddress(0x200)));
  CHECK_EQ(3u, map.size());

  map.Clear();
  CHECK_EQ(0u, map.size());
  CHECK_EQ(0u, map.GetTraceNodeId(ToAddress(0x400)));
}

static const v8::AllocationProfile::Node* FindAllocationProfileNode(
    v8::Isolate* isolate, v8::AllocationProfile& profile,
    const Vector<const char*>& names) {
  v8::AllocationProfile::Node* node = profile.GetRootNode();
  for (int i = 0; node != nullptr && i < names.length(); ++i) {
    const char* name = names[i];
    auto children = node->children;
    node = nullptr;
    for (v8::AllocationProfile::Node* child : children) {
      v8::String::Utf8Value child_name(isolate, child->name);
      if (strcmp(*child_name, name) == 0) {
        node = child;
        break;
      }
    }
  }
  return node;
}

static void CheckNoZeroCountNodes(v8::AllocationProfile::Node* node) {
  for (auto alloc : node->allocations) {
    CHECK_GT(alloc.count, 0u);
  }
  for (auto child : node->children) {
    CheckNoZeroCountNodes(child);
  }
}

TEST(SamplingHeapProfiler) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  // Turn off always_opt. Inlining can cause stack traces to be shorter than
  // what we expect in this test.
  v8::internal::FLAG_always_opt = false;

  // Suppress randomness to avoid flakiness in tests.
  v8::internal::FLAG_sampling_heap_profiler_suppress_randomness = true;

  const char* script_source =
      "var A = [];\n"
      "function bar(size) { return new Array(size); }\n"
      "var foo = function() {\n"
      "  for (var i = 0; i < 1024; ++i) {\n"
      "    A[i] = bar(1024);\n"
      "  }\n"
      "}\n"
      "foo();";

  // Sample should be empty if requested before sampling has started.
  {
    v8::AllocationProfile* profile = heap_profiler->GetAllocationProfile();
    CHECK_NULL(profile);
  }

  int count_1024 = 0;
  {
    heap_profiler->StartSamplingHeapProfiler(1024);
    CompileRun(script_source);

    std::unique_ptr<v8::AllocationProfile> profile(
        heap_profiler->GetAllocationProfile());
    CHECK(profile);

    const char* names[] = {"", "foo", "bar"};
    auto node_bar = FindAllocationProfileNode(env->GetIsolate(), *profile,
                                              ArrayVector(names));
    CHECK(node_bar);

    // Count the number of allocations we sampled from bar.
    for (auto allocation : node_bar->allocations) {
      count_1024 += allocation.count;
    }

    heap_profiler->StopSamplingHeapProfiler();
  }

  // Samples should get cleared once sampling is stopped.
  {
    v8::AllocationProfile* profile = heap_profiler->GetAllocationProfile();
    CHECK_NULL(profile);
  }

  // Sampling at a higher rate should give us similar numbers of objects.
  {
    heap_profiler->StartSamplingHeapProfiler(128);
    CompileRun(script_source);

    std::unique_ptr<v8::AllocationProfile> profile(
        heap_profiler->GetAllocationProfile());
    CHECK(profile);

    const char* names[] = {"", "foo", "bar"};
    auto node_bar = FindAllocationProfileNode(env->GetIsolate(), *profile,
                                              ArrayVector(names));
    CHECK(node_bar);

    // Count the number of allocations we sampled from bar.
    int count_128 = 0;
    for (auto allocation : node_bar->allocations) {
      count_128 += allocation.count;
    }

    // We should have similar unsampled counts of allocations. Though
    // we will sample different numbers of objects at different rates,
    // the unsampling process should produce similar final estimates
    // at the true number of allocations. However, the process to
    // determine these unsampled counts is probabilisitic so we need to
    // account for error.
    double max_count = std::max(count_128, count_1024);
    double min_count = std::min(count_128, count_1024);
    double percent_difference = (max_count - min_count) / min_count;
    CHECK_LT(percent_difference, 0.15);

    heap_profiler->StopSamplingHeapProfiler();
  }

  // A more complicated test cases with deeper call graph and dynamically
  // generated function names.
  {
    heap_profiler->StartSamplingHeapProfiler(64);
    CompileRun(record_trace_tree_source);

    std::unique_ptr<v8::AllocationProfile> profile(
        heap_profiler->GetAllocationProfile());
    CHECK(profile);

    const char* names1[] = {"", "start", "f_0_0", "f_0_1", "f_0_2"};
    auto node1 = FindAllocationProfileNode(env->GetIsolate(), *profile,
                                           ArrayVector(names1));
    CHECK(node1);

    const char* names2[] = {"", "generateFunctions"};
    auto node2 = FindAllocationProfileNode(env->GetIsolate(), *profile,
                                           ArrayVector(names2));
    CHECK(node2);

    heap_profiler->StopSamplingHeapProfiler();
  }

  // A test case with scripts unloaded before profile gathered
  {
    heap_profiler->StartSamplingHeapProfiler(64);
    CompileRun(
        "for (var i = 0; i < 1024; i++) {\n"
        "  eval(\"new Array(100)\");\n"
        "}\n");

    CcTest::CollectAllGarbage();

    std::unique_ptr<v8::AllocationProfile> profile(
        heap_profiler->GetAllocationProfile());
    CHECK(profile);

    CheckNoZeroCountNodes(profile->GetRootNode());

    heap_profiler->StopSamplingHeapProfiler();
  }
}


TEST(SamplingHeapProfilerApiAllocation) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  // Suppress randomness to avoid flakiness in tests.
  v8::internal::FLAG_sampling_heap_profiler_suppress_randomness = true;

  heap_profiler->StartSamplingHeapProfiler(256);

  for (int i = 0; i < 8 * 1024; ++i) v8::Object::New(env->GetIsolate());

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  CHECK(profile);
  const char* names[] = {"(V8 API)"};
  auto node = FindAllocationProfileNode(env->GetIsolate(), *profile,
                                        ArrayVector(names));
  CHECK(node);

  heap_profiler->StopSamplingHeapProfiler();
}

TEST(SamplingHeapProfilerLeftTrimming) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  // Suppress randomness to avoid flakiness in tests.
  v8::internal::FLAG_sampling_heap_profiler_suppress_randomness = true;

  heap_profiler->StartSamplingHeapProfiler(64);

  CompileRun(
      "for (var j = 0; j < 500; ++j) {\n"
      "  var a = [];\n"
      "  for (var i = 0; i < 5; ++i)\n"
      "      a[i] = i;\n"
      "  for (var i = 0; i < 3; ++i)\n"
      "      a.shift();\n"
      "}\n");

  CcTest::CollectGarbage(v8::internal::NEW_SPACE);
  // Should not crash.

  heap_profiler->StopSamplingHeapProfiler();
}

TEST(SamplingHeapProfilerPretenuredInlineAllocations) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_expose_gc = true;

  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_optimizer() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction ||
      i::FLAG_stress_incremental_marking) {
    return;
  }

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  // Suppress randomness to avoid flakiness in tests.
  v8::internal::FLAG_sampling_heap_profiler_suppress_randomness = true;

  // Grow new space until maximum capacity reached.
  while (!CcTest::heap()->new_space()->IsAtMaximumCapacity()) {
    CcTest::heap()->new_space()->Grow();
  }

  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "var number_elements = %d;"
              "var elements = new Array(number_elements);"
              "function f() {"
              "  for (var i = 0; i < number_elements; i++) {"
              "    elements[i] = [{}, {}, {}];"
              "  }"
              "  return elements[number_elements - 1];"
              "};"
              "f(); gc();"
              "f(); f();"
              "%%OptimizeFunctionOnNextCall(f);"
              "f();"
              "f;",
              i::AllocationSite::kPretenureMinimumCreated + 1);

  v8::Local<v8::Function> f =
      v8::Local<v8::Function>::Cast(CompileRun(source.start()));

  // Make sure the function is producing pre-tenured objects.
  auto res = f->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  i::Handle<i::JSObject> o = i::Handle<i::JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res)));
  CHECK(CcTest::heap()->InOldSpace(o->elements()));
  CHECK(CcTest::heap()->InOldSpace(*o));

  // Call the function and profile it.
  heap_profiler->StartSamplingHeapProfiler(64);
  for (int i = 0; i < 80; ++i) {
    f->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  }

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  CHECK(profile);
  heap_profiler->StopSamplingHeapProfiler();

  const char* names[] = {"f"};
  auto node_f = FindAllocationProfileNode(env->GetIsolate(), *profile,
                                          ArrayVector(names));
  CHECK(node_f);

  int count = 0;
  for (auto allocation : node_f->allocations) {
    count += allocation.count;
  }

  CHECK_GE(count, 8000);
}

TEST(SamplingHeapProfilerLargeInterval) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  // Suppress randomness to avoid flakiness in tests.
  v8::internal::FLAG_sampling_heap_profiler_suppress_randomness = true;

  heap_profiler->StartSamplingHeapProfiler(512 * 1024);

  for (int i = 0; i < 8 * 1024; ++i) {
    CcTest::i_isolate()->factory()->NewFixedArray(1024);
  }

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  CHECK(profile);
  const char* names[] = {"(EXTERNAL)"};
  auto node = FindAllocationProfileNode(env->GetIsolate(), *profile,
                                        ArrayVector(names));
  CHECK(node);

  heap_profiler->StopSamplingHeapProfiler();
}

TEST(SamplingHeapProfilerSampleDuringDeopt) {
  i::FLAG_allow_natives_syntax = true;

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  // Suppress randomness to avoid flakiness in tests.
  v8::internal::FLAG_sampling_heap_profiler_suppress_randomness = true;

  // Small sample interval to force each object to be sampled.
  heap_profiler->StartSamplingHeapProfiler(i::kPointerSize);

  // Lazy deopt from runtime call from inlined callback function.
  const char* source =
      "var b = "
      "  [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25];"
      "(function f() {"
      "  var result = 0;"
      "  var lazyDeopt = function(deopt) {"
      "    var callback = function(v,i,o) {"
      "      result += i;"
      "      if (i == 13 && deopt) {"
      "          %DeoptimizeNow();"
      "      }"
      "      return v;"
      "    };"
      "    b.map(callback);"
      "  };"
      "  lazyDeopt();"
      "  lazyDeopt();"
      "  %OptimizeFunctionOnNextCall(lazyDeopt);"
      "  lazyDeopt();"
      "  lazyDeopt(true);"
      "  lazyDeopt();"
      "})();";

  CompileRun(source);
  // Should not crash.

  std::unique_ptr<v8::AllocationProfile> profile(
      heap_profiler->GetAllocationProfile());
  CHECK(profile);
  heap_profiler->StopSamplingHeapProfiler();
}

TEST(HeapSnapshotPrototypeNotJSReceiver) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun(
      "function object() {}"
      "object.prototype = 42;");
  const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();
  CHECK(ValidateSnapshot(snapshot));
}
