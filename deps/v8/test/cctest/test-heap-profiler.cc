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

#include "src/v8.h"

#include "include/v8-profiler.h"
#include "src/allocation-tracker.h"
#include "src/debug.h"
#include "src/hashmap.h"
#include "src/heap-profiler.h"
#include "src/snapshot.h"
#include "src/utils-inl.h"
#include "test/cctest/cctest.h"

using i::AllocationTraceNode;
using i::AllocationTraceTree;
using i::AllocationTracker;
using i::HashMap;
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

  static bool AddressesMatch(void* key1, void* key2) {
    return key1 == key2;
  }

  void CheckAllReachables(i::HeapEntry* root) {
    i::HashMap visited(AddressesMatch);
    i::List<i::HeapEntry*> list(10);
    list.Add(root);
    CheckEntry(root);
    while (!list.is_empty()) {
      i::HeapEntry* entry = list.RemoveLast();
      i::Vector<i::HeapGraphEdge*> children = entry->children();
      for (int i = 0; i < children.length(); ++i) {
        if (children[i]->type() == i::HeapGraphEdge::kShortcut) continue;
        i::HeapEntry* child = children[i]->to();
        i::HashMap::Entry* entry = visited.Lookup(
            reinterpret_cast<void*>(child),
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(child)),
            true);
        if (entry->value)
          continue;
        entry->value = reinterpret_cast<void*>(1);
        list.Add(child);
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


static const v8::HeapGraphNode* GetProperty(const v8::HeapGraphNode* node,
                                            v8::HeapGraphEdge::Type type,
                                            const char* name) {
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = node->GetChild(i);
    v8::String::Utf8Value prop_name(prop->GetName());
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
      v8::String::Utf8Value node_name(node->GetName());
      if (strcmp(contents, *node_name) == 0) return true;
    }
  }
  return false;
}


static bool AddressesMatch(void* key1, void* key2) {
  return key1 == key2;
}


// Check that snapshot has no unretained entries except root.
static bool ValidateSnapshot(const v8::HeapSnapshot* snapshot, int depth = 3) {
  i::HeapSnapshot* heap_snapshot = const_cast<i::HeapSnapshot*>(
      reinterpret_cast<const i::HeapSnapshot*>(snapshot));

  i::HashMap visited(AddressesMatch);
  i::List<i::HeapGraphEdge>& edges = heap_snapshot->edges();
  for (int i = 0; i < edges.length(); ++i) {
    i::HashMap::Entry* entry = visited.Lookup(
        reinterpret_cast<void*>(edges[i].to()),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(edges[i].to())),
        true);
    uint32_t ref_count = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(entry->value));
    entry->value = reinterpret_cast<void*>(ref_count + 1);
  }
  uint32_t unretained_entries_count = 0;
  i::List<i::HeapEntry>& entries = heap_snapshot->entries();
  for (int i = 0; i < entries.length(); ++i) {
    i::HashMap::Entry* entry = visited.Lookup(
        reinterpret_cast<void*>(&entries[i]),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&entries[i])),
        false);
    if (!entry && entries[i].id() != 1) {
        entries[i].Print("entry with no retainer", "", depth, 0);
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
  const v8::HeapSnapshot* snapshot_env2 =
      heap_profiler->TakeHeapSnapshot(v8_str("env2"));
  CHECK(ValidateSnapshot(snapshot_env2));
  const v8::HeapGraphNode* global_env2 = GetGlobalObject(snapshot_env2);

  // Verify, that JS global object of env2 has '..2' properties.
  const v8::HeapGraphNode* a2_node =
      GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "a2");
  CHECK_NE(NULL, a2_node);
  CHECK_NE(
      NULL, GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "b2_1"));
  CHECK_NE(
      NULL, GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "b2_2"));
  CHECK_NE(NULL, GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "c2"));

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
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("sizes"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* x =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "x");
  CHECK_NE(NULL, x);
  const v8::HeapGraphNode* x1 =
      GetProperty(x, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, x1);
  const v8::HeapGraphNode* x2 =
      GetProperty(x, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, x2);

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
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("sizes"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* f =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "boundFunction");
  CHECK(f);
  CHECK_EQ(v8::String::NewFromUtf8(env->GetIsolate(), "native_bind"),
           f->GetName());
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
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function A() { }\n"
      "a = new A;");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("children"));
  CHECK(ValidateSnapshot(snapshot));
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
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function lazy(x) { return x - 1; }\n"
      "function compiled(x) { return x + 1; }\n"
      "var anonymous = (function() { return function() { return 0; } })();\n"
      "compiled(1)");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("code"));
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* compiled =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "compiled");
  CHECK_NE(NULL, compiled);
  CHECK_EQ(v8::HeapGraphNode::kClosure, compiled->GetType());
  const v8::HeapGraphNode* lazy =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "lazy");
  CHECK_NE(NULL, lazy);
  CHECK_EQ(v8::HeapGraphNode::kClosure, lazy->GetType());
  const v8::HeapGraphNode* anonymous =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "anonymous");
  CHECK_NE(NULL, anonymous);
  CHECK_EQ(v8::HeapGraphNode::kClosure, anonymous->GetType());
  v8::String::Utf8Value anonymous_name(anonymous->GetName());
  CHECK_EQ("", *anonymous_name);

  // Find references to code.
  const v8::HeapGraphNode* compiled_code =
      GetProperty(compiled, v8::HeapGraphEdge::kInternal, "shared");
  CHECK_NE(NULL, compiled_code);
  const v8::HeapGraphNode* lazy_code =
      GetProperty(lazy, v8::HeapGraphEdge::kInternal, "shared");
  CHECK_NE(NULL, lazy_code);

  // Check that there's no strong next_code_link. There might be a weak one
  // but might be not, so we can't check that fact.
  const v8::HeapGraphNode* code =
      GetProperty(compiled_code, v8::HeapGraphEdge::kInternal, "code");
  CHECK_NE(NULL, code);
  const v8::HeapGraphNode* next_code_link =
      GetProperty(code, v8::HeapGraphEdge::kInternal, "code");
  CHECK_EQ(NULL, next_code_link);

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
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun(
      "a = 1;    // a is Smi\n"
      "b = 2.5;  // b is HeapNumber");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("numbers"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_EQ(NULL, GetProperty(global, v8::HeapGraphEdge::kProperty, "a"));
  const v8::HeapGraphNode* b =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, b);
  CHECK_EQ(v8::HeapGraphNode::kHeapNumber, b->GetType());
}


TEST(HeapSnapshotSlicedString) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun(
      "parent_string = \"123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789."
      "123456789.123456789.123456789.123456789.123456789.\";"
      "child_string = parent_string.slice(100);");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("strings"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* parent_string =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "parent_string");
  CHECK_NE(NULL, parent_string);
  const v8::HeapGraphNode* child_string =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "child_string");
  CHECK_NE(NULL, child_string);
  CHECK_EQ(v8::HeapGraphNode::kSlicedString, child_string->GetType());
  const v8::HeapGraphNode* parent =
      GetProperty(child_string, v8::HeapGraphEdge::kInternal, "parent");
  CHECK_EQ(parent_string, parent);
  heap_profiler->DeleteAllHeapSnapshots();
}


TEST(HeapSnapshotConsString) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->SetInternalFieldCount(1);
  LocalContext env(NULL, global_template);
  v8::Handle<v8::Object> global_proxy = env->Global();
  v8::Handle<v8::Object> global = global_proxy->GetPrototype().As<v8::Object>();
  CHECK_EQ(1, global->InternalFieldCount());

  i::Factory* factory = CcTest::i_isolate()->factory();
  i::Handle<i::String> first = factory->NewStringFromStaticChars("0123456789");
  i::Handle<i::String> second = factory->NewStringFromStaticChars("0123456789");
  i::Handle<i::String> cons_string =
      factory->NewConsString(first, second).ToHandleChecked();

  global->SetInternalField(0, v8::ToApiHandle<v8::String>(cons_string));

  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("cons_strings"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global_node = GetGlobalObject(snapshot);

  const v8::HeapGraphNode* string_node =
      GetProperty(global_node, v8::HeapGraphEdge::kInternal, "0");
  CHECK_NE(NULL, string_node);
  CHECK_EQ(v8::HeapGraphNode::kConsString, string_node->GetType());

  const v8::HeapGraphNode* first_node =
      GetProperty(string_node, v8::HeapGraphEdge::kInternal, "first");
  CHECK_EQ(v8::HeapGraphNode::kString, first_node->GetType());

  const v8::HeapGraphNode* second_node =
      GetProperty(string_node, v8::HeapGraphEdge::kInternal, "second");
  CHECK_EQ(v8::HeapGraphNode::kString, second_node->GetType());

  heap_profiler->DeleteAllHeapSnapshots();
}


TEST(HeapSnapshotSymbol) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("a = Symbol('mySymbol');\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("Symbol"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* a =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, a);
  CHECK_EQ(a->GetType(), v8::HeapGraphNode::kSymbol);
  CHECK_EQ(v8_str("symbol"), a->GetName());
  const v8::HeapGraphNode* name =
      GetProperty(a, v8::HeapGraphEdge::kInternal, "name");
  CHECK_NE(NULL, name);
  CHECK_EQ(v8_str("mySymbol"), name->GetName());
}


TEST(HeapSnapshotWeakCollection) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "k = {}; v = {}; s = 'str';\n"
      "ws = new WeakSet(); ws.add(k); ws.add(v); ws[s] = s;\n"
      "wm = new WeakMap(); wm.set(k, v); wm[s] = s;\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("WeakCollections"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* k =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "k");
  CHECK_NE(NULL, k);
  const v8::HeapGraphNode* v =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "v");
  CHECK_NE(NULL, v);
  const v8::HeapGraphNode* s =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "s");
  CHECK_NE(NULL, s);

  const v8::HeapGraphNode* ws =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "ws");
  CHECK_NE(NULL, ws);
  CHECK_EQ(v8::HeapGraphNode::kObject, ws->GetType());
  CHECK_EQ(v8_str("WeakSet"), ws->GetName());

  const v8::HeapGraphNode* ws_table =
      GetProperty(ws, v8::HeapGraphEdge::kInternal, "table");
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
      GetProperty(ws, v8::HeapGraphEdge::kProperty, "str");
  CHECK_NE(NULL, ws_s);
  CHECK_EQ(static_cast<int>(s->GetId()), static_cast<int>(ws_s->GetId()));

  const v8::HeapGraphNode* wm =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "wm");
  CHECK_NE(NULL, wm);
  CHECK_EQ(v8::HeapGraphNode::kObject, wm->GetType());
  CHECK_EQ(v8_str("WeakMap"), wm->GetName());

  const v8::HeapGraphNode* wm_table =
      GetProperty(wm, v8::HeapGraphEdge::kInternal, "table");
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
  CHECK_EQ(2, weak_entries);
  const v8::HeapGraphNode* wm_s =
      GetProperty(wm, v8::HeapGraphEdge::kProperty, "str");
  CHECK_NE(NULL, wm_s);
  CHECK_EQ(static_cast<int>(s->GetId()), static_cast<int>(wm_s->GetId()));
}


TEST(HeapSnapshotCollection) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "k = {}; v = {}; s = 'str';\n"
      "set = new Set(); set.add(k); set.add(v); set[s] = s;\n"
      "map = new Map(); map.set(k, v); map[s] = s;\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("Collections"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* k =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "k");
  CHECK_NE(NULL, k);
  const v8::HeapGraphNode* v =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "v");
  CHECK_NE(NULL, v);
  const v8::HeapGraphNode* s =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "s");
  CHECK_NE(NULL, s);

  const v8::HeapGraphNode* set =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "set");
  CHECK_NE(NULL, set);
  CHECK_EQ(v8::HeapGraphNode::kObject, set->GetType());
  CHECK_EQ(v8_str("Set"), set->GetName());

  const v8::HeapGraphNode* set_table =
      GetProperty(set, v8::HeapGraphEdge::kInternal, "table");
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
      GetProperty(set, v8::HeapGraphEdge::kProperty, "str");
  CHECK_NE(NULL, set_s);
  CHECK_EQ(static_cast<int>(s->GetId()), static_cast<int>(set_s->GetId()));

  const v8::HeapGraphNode* map =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "map");
  CHECK_NE(NULL, map);
  CHECK_EQ(v8::HeapGraphNode::kObject, map->GetType());
  CHECK_EQ(v8_str("Map"), map->GetName());

  const v8::HeapGraphNode* map_table =
      GetProperty(map, v8::HeapGraphEdge::kInternal, "table");
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
      GetProperty(map, v8::HeapGraphEdge::kProperty, "str");
  CHECK_NE(NULL, map_s);
  CHECK_EQ(static_cast<int>(s->GetId()), static_cast<int>(map_s->GetId()));
}


TEST(HeapSnapshotInternalReferences) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->SetInternalFieldCount(2);
  LocalContext env(NULL, global_template);
  v8::Handle<v8::Object> global_proxy = env->Global();
  v8::Handle<v8::Object> global = global_proxy->GetPrototype().As<v8::Object>();
  CHECK_EQ(2, global->InternalFieldCount());
  v8::Local<v8::Object> obj = v8::Object::New(isolate);
  global->SetInternalField(0, v8_num(17));
  global->SetInternalField(1, obj);
  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("internals"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global_node = GetGlobalObject(snapshot);
  // The first reference will not present, because it's a Smi.
  CHECK_EQ(NULL, GetProperty(global_node, v8::HeapGraphEdge::kInternal, "0"));
  // The second reference is to an object.
  CHECK_NE(NULL, GetProperty(global_node, v8::HeapGraphEdge::kInternal, "1"));
}


// Trying to introduce a check helper for uint32_t causes many
// overloading ambiguities, so it seems easier just to cast
// them to a signed type.
#define CHECK_EQ_SNAPSHOT_OBJECT_ID(a, b) \
  CHECK_EQ(static_cast<int32_t>(a), static_cast<int32_t>(b))
#define CHECK_NE_SNAPSHOT_OBJECT_ID(a, b) \
  CHECK((a) != (b))  // NOLINT

TEST(HeapSnapshotAddressReuse) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function A() {}\n"
      "var a = [];\n"
      "for (var i = 0; i < 10000; ++i)\n"
      "  a[i] = new A();\n");
  const v8::HeapSnapshot* snapshot1 =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot1"));
  CHECK(ValidateSnapshot(snapshot1));
  v8::SnapshotObjectId maxId1 = snapshot1->GetMaxSnapshotJSObjectId();

  CompileRun(
      "for (var i = 0; i < 10000; ++i)\n"
      "  a[i] = new A();\n");
  CcTest::heap()->CollectAllGarbage(i::Heap::kNoGCFlags);

  const v8::HeapSnapshot* snapshot2 =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot2"));
  CHECK(ValidateSnapshot(snapshot2));
  const v8::HeapGraphNode* global2 = GetGlobalObject(snapshot2);

  const v8::HeapGraphNode* array_node =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, array_node);
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
  const v8::HeapSnapshot* snapshot1 =
      heap_profiler->TakeHeapSnapshot(v8_str("s1"));
  CHECK(ValidateSnapshot(snapshot1));

  CompileRun(
      "for (var i = 0; i < 1; ++i)\n"
      "  a.shift();\n");

  CcTest::heap()->CollectAllGarbage(i::Heap::kNoGCFlags);

  const v8::HeapSnapshot* snapshot2 =
      heap_profiler->TakeHeapSnapshot(v8_str("s2"));
  CHECK(ValidateSnapshot(snapshot2));

  const v8::HeapGraphNode* global1 = GetGlobalObject(snapshot1);
  const v8::HeapGraphNode* global2 = GetGlobalObject(snapshot2);
  CHECK_NE_SNAPSHOT_OBJECT_ID(0, global1->GetId());
  CHECK_EQ_SNAPSHOT_OBJECT_ID(global1->GetId(), global2->GetId());

  const v8::HeapGraphNode* a1 =
      GetProperty(global1, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, a1);
  const v8::HeapGraphNode* k1 =
      GetProperty(a1, v8::HeapGraphEdge::kInternal, "elements");
  CHECK_NE(NULL, k1);
  const v8::HeapGraphNode* a2 =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, a2);
  const v8::HeapGraphNode* k2 =
      GetProperty(a2, v8::HeapGraphEdge::kInternal, "elements");
  CHECK_NE(NULL, k2);

  CHECK_EQ_SNAPSHOT_OBJECT_ID(a1->GetId(), a2->GetId());
  CHECK_EQ_SNAPSHOT_OBJECT_ID(k1->GetId(), k2->GetId());
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
  v8::Local<v8::String> s1_str = v8_str("s1");
  v8::Local<v8::String> s2_str = v8_str("s2");
  const v8::HeapSnapshot* snapshot1 =
      heap_profiler->TakeHeapSnapshot(s1_str);
  CHECK(ValidateSnapshot(snapshot1));

  CcTest::heap()->CollectAllGarbage(i::Heap::kNoGCFlags);

  const v8::HeapSnapshot* snapshot2 =
      heap_profiler->TakeHeapSnapshot(s2_str);
  CHECK(ValidateSnapshot(snapshot2));

  CHECK_GT(snapshot1->GetMaxSnapshotJSObjectId(), 7000);
  CHECK(snapshot1->GetMaxSnapshotJSObjectId() <=
        snapshot2->GetMaxSnapshotJSObjectId());

  const v8::HeapGraphNode* global1 = GetGlobalObject(snapshot1);
  const v8::HeapGraphNode* global2 = GetGlobalObject(snapshot2);
  CHECK_NE_SNAPSHOT_OBJECT_ID(0, global1->GetId());
  CHECK_EQ_SNAPSHOT_OBJECT_ID(global1->GetId(), global2->GetId());
  const v8::HeapGraphNode* A1 =
      GetProperty(global1, v8::HeapGraphEdge::kProperty, "A");
  CHECK_NE(NULL, A1);
  const v8::HeapGraphNode* A2 =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "A");
  CHECK_NE(NULL, A2);
  CHECK_NE_SNAPSHOT_OBJECT_ID(0, A1->GetId());
  CHECK_EQ_SNAPSHOT_OBJECT_ID(A1->GetId(), A2->GetId());
  const v8::HeapGraphNode* B1 =
      GetProperty(global1, v8::HeapGraphEdge::kProperty, "B");
  CHECK_NE(NULL, B1);
  const v8::HeapGraphNode* B2 =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "B");
  CHECK_NE(NULL, B2);
  CHECK_NE_SNAPSHOT_OBJECT_ID(0, B1->GetId());
  CHECK_EQ_SNAPSHOT_OBJECT_ID(B1->GetId(), B2->GetId());
  const v8::HeapGraphNode* a1 =
      GetProperty(global1, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, a1);
  const v8::HeapGraphNode* a2 =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, a2);
  CHECK_NE_SNAPSHOT_OBJECT_ID(0, a1->GetId());
  CHECK_EQ_SNAPSHOT_OBJECT_ID(a1->GetId(), a2->GetId());
  const v8::HeapGraphNode* b1 =
      GetProperty(global1, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, b1);
  const v8::HeapGraphNode* b2 =
      GetProperty(global2, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, b2);
  CHECK_NE_SNAPSHOT_OBJECT_ID(0, b1->GetId());
  CHECK_EQ_SNAPSHOT_OBJECT_ID(b1->GetId(), b2->GetId());
}


TEST(HeapSnapshotRootPreservedAfterSorting) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("s"));
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
    DCHECK(false);
    return kAbort;
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
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

#define STRING_LITERAL_FOR_TEST \
  "\"String \\n\\r\\u0008\\u0081\\u0101\\u0801\\u8001\""
  CompileRun(
      "function A(s) { this.s = s; }\n"
      "function B(x) { this.x = x; }\n"
      "var a = new A(" STRING_LITERAL_FOR_TEST ");\n"
      "var b = new B(a);");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("json"));
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
      v8::String::NewExternal(env->GetIsolate(), json_res);
  env->Global()->Set(v8_str("json_snapshot"), json_string);
  v8::Local<v8::Value> snapshot_parse_result = CompileRun(
      "var parsed = JSON.parse(json_snapshot); true;");
  CHECK(!snapshot_parse_result.IsEmpty());

  // Verify that snapshot object has required fields.
  v8::Local<v8::Object> parsed_snapshot =
      env->Global()->Get(v8_str("parsed"))->ToObject();
  CHECK(parsed_snapshot->Has(v8_str("snapshot")));
  CHECK(parsed_snapshot->Has(v8_str("nodes")));
  CHECK(parsed_snapshot->Has(v8_str("edges")));
  CHECK(parsed_snapshot->Has(v8_str("strings")));

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
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("abort"));
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
    DCHECK(false);
    return kAbort;
  }
  virtual WriteResult WriteHeapStatsChunk(v8::HeapStatsUpdate* buffer,
                                          int updates_written) {
    ++intervals_count_;
    DCHECK(updates_written);
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
    v8::SnapshotObjectId* object_id = NULL) {
  TestStatsStream stream;
  v8::SnapshotObjectId last_seen_id = heap_profiler->GetHeapStats(&stream);
  if (object_id)
    *object_id = last_seen_id;
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
    CcTest::heap()->CollectAllGarbage(i::Heap::kNoGCFlags);
  }

  v8::SnapshotObjectId initial_id;
  {
    // Single chunk of data expected in update. Initial data.
    TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler,
                                                      &initial_id);
    CHECK_EQ(1, stats_update.intervals_count());
    CHECK_EQ(1, stats_update.updates_written());
    CHECK_LT(0, stats_update.entries_size());
    CHECK_EQ(0, stats_update.first_interval_index());
  }

  // No data expected in update because nothing has happened.
  v8::SnapshotObjectId same_id;
  CHECK_EQ(0, GetHeapStatsUpdate(heap_profiler, &same_id).updates_written());
  CHECK_EQ_SNAPSHOT_OBJECT_ID(initial_id, same_id);

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
      CHECK_LT(0, stats_update.entries_size());
      CHECK_EQ(1, stats_update.entries_count());
      CHECK_EQ(2, stats_update.first_interval_index());
    }

    // No data expected in update because nothing happened.
    v8::SnapshotObjectId last_id;
    CHECK_EQ(0, GetHeapStatsUpdate(heap_profiler, &last_id).updates_written());
    CHECK_EQ_SNAPSHOT_OBJECT_ID(additional_string_id, last_id);

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
          CHECK_LT(0, entries_size = stats_update.entries_size());
          CHECK_EQ(3, stats_update.entries_count());
          CHECK_EQ(4, stats_update.first_interval_index());
        }
      }

      {
        // Single chunk of data with two left entries expected in update.
        TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
        CHECK_EQ(1, stats_update.intervals_count());
        CHECK_EQ(1, stats_update.updates_written());
        CHECK_GT(entries_size, stats_update.entries_size());
        CHECK_EQ(1, stats_update.entries_count());
        // Two strings from forth interval were released.
        CHECK_EQ(4, stats_update.first_interval_index());
      }
    }

    {
      // Single chunk of data with 0 left entries expected in update.
      TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
      CHECK_EQ(1, stats_update.intervals_count());
      CHECK_EQ(1, stats_update.updates_written());
      CHECK_EQ(0, stats_update.entries_size());
      CHECK_EQ(0, stats_update.entries_count());
      // The last string from forth interval was released.
      CHECK_EQ(4, stats_update.first_interval_index());
    }
  }
  {
    // Single chunk of data with 0 left entries expected in update.
    TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
    CHECK_EQ(1, stats_update.intervals_count());
    CHECK_EQ(1, stats_update.updates_written());
    CHECK_EQ(0, stats_update.entries_size());
    CHECK_EQ(0, stats_update.entries_count());
    // The only string from the second interval was released.
    CHECK_EQ(2, stats_update.first_interval_index());
  }

  v8::Local<v8::Array> array = v8::Array::New(env->GetIsolate());
  CHECK_EQ(0, array->Length());
  // Force array's buffer allocation.
  array->Set(2, v8_num(7));

  uint32_t entries_size;
  {
    // Single chunk of data with 2 entries expected in update.
    TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
    CHECK_EQ(1, stats_update.intervals_count());
    CHECK_EQ(1, stats_update.updates_written());
    CHECK_LT(0, entries_size = stats_update.entries_size());
    // They are the array and its buffer.
    CHECK_EQ(2, stats_update.entries_count());
    CHECK_EQ(8, stats_update.first_interval_index());
  }

  for (int i = 0; i < 100; ++i)
    array->Set(i, v8_num(i));

  {
    // Single chunk of data with 1 entry expected in update.
    TestStatsStream stats_update = GetHeapStatsUpdate(heap_profiler);
    CHECK_EQ(1, stats_update.intervals_count());
    // The first interval was changed because old buffer was collected.
    // The second interval was changed because new buffer was allocated.
    CHECK_EQ(2, stats_update.updates_written());
    CHECK_LT(entries_size, stats_update.entries_size());
    CHECK_EQ(2, stats_update.entries_count());
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
  v8::Handle<v8::Object> objects[kLength];
  v8::SnapshotObjectId ids[kLength];

  heap_profiler->StartTrackingHeapObjects(false);

  for (int i = 0; i < kLength; i++) {
    objects[i] = v8::Object::New(isolate);
  }
  GetHeapStatsUpdate(heap_profiler);

  for (int i = 0; i < kLength; i++) {
    v8::SnapshotObjectId id = heap_profiler->GetObjectId(objects[i]);
    CHECK_NE(v8::HeapProfiler::kUnknownObjectId, static_cast<int>(id));
    ids[i] = id;
  }

  heap_profiler->StopTrackingHeapObjects();
  CcTest::heap()->CollectAllAvailableGarbage();

  for (int i = 0; i < kLength; i++) {
    v8::SnapshotObjectId id = heap_profiler->GetObjectId(objects[i]);
    CHECK_EQ(static_cast<int>(ids[i]), static_cast<int>(id));
    v8::Handle<v8::Value> obj = heap_profiler->FindObjectById(ids[i]);
    CHECK_EQ(objects[i], obj);
  }

  heap_profiler->ClearObjectIds();
  for (int i = 0; i < kLength; i++) {
    v8::SnapshotObjectId id = heap_profiler->GetObjectId(objects[i]);
    CHECK_EQ(v8::HeapProfiler::kUnknownObjectId, static_cast<int>(id));
    v8::Handle<v8::Value> obj = heap_profiler->FindObjectById(ids[i]);
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
    CHECK_EQ_SNAPSHOT_OBJECT_ID(prop->GetToNode()->GetId(), child->GetId());
    CHECK_EQ(prop->GetToNode(), child);
    CheckChildrenIds(snapshot, child, level + 1, max_level);
  }
}


TEST(HeapSnapshotGetNodeById) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("id"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* root = snapshot->GetRoot();
  CheckChildrenIds(snapshot, root, 0, 3);
  // Check a big id, which should not exist yet.
  CHECK_EQ(NULL, snapshot->GetNodeById(0x1000000UL));
}


TEST(HeapSnapshotGetSnapshotObjectId) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("globalObject = {};\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("get_snapshot_object_id"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* global_object =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "globalObject");
  CHECK(global_object);

  v8::Local<v8::Value> globalObjectHandle = env->Global()->Get(
      v8::String::NewFromUtf8(env->GetIsolate(), "globalObject"));
  CHECK(!globalObjectHandle.IsEmpty());
  CHECK(globalObjectHandle->IsObject());

  v8::SnapshotObjectId id = heap_profiler->GetObjectId(globalObjectHandle);
  CHECK_NE(static_cast<int>(v8::HeapProfiler::kUnknownObjectId),
           id);
  CHECK_EQ(static_cast<int>(id), global_object->GetId());
}


TEST(HeapSnapshotUnknownSnapshotObjectId) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("globalObject = {};\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("unknown_object_id"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* node =
      snapshot->GetNodeById(v8::HeapProfiler::kUnknownObjectId);
  CHECK_EQ(NULL, node);
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
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  const int snapshots_count = heap_profiler->GetSnapshotCount();
  TestActivityControl aborting_control(1);
  const v8::HeapSnapshot* no_snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("abort"),
                                     &aborting_control);
  CHECK_EQ(NULL, no_snapshot);
  CHECK_EQ(snapshots_count, heap_profiler->GetSnapshotCount());
  CHECK_GT(aborting_control.total(), aborting_control.done());

  TestActivityControl control(-1);  // Don't abort.
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("full"),
                                     &control);
  CHECK(ValidateSnapshot(snapshot));

  CHECK_NE(NULL, snapshot);
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
        v8::String::Utf8Value utf8(wrapper);
        if (strcmp(*utf8, "AAA") == 0)
          return new TestRetainedObjectInfo(1, "aaa-group", "aaa", 100);
        else if (strcmp(*utf8, "BBB") == 0)
          return new TestRetainedObjectInfo(1, "aaa-group", "aaa", 100);
      }
    } else if (class_id == 2) {
      if (wrapper->IsString()) {
        v8::String::Utf8Value utf8(wrapper);
        if (strcmp(*utf8, "CCC") == 0)
          return new TestRetainedObjectInfo(2, "ccc-group", "ccc");
      }
    }
    CHECK(false);
    return NULL;
  }

  static i::List<TestRetainedObjectInfo*> instances;

 private:
  bool disposed_;
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
  CHECK_EQ(0, TestRetainedObjectInfo::instances.length());
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("retained"));
  CHECK(ValidateSnapshot(snapshot));

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
    isolate_ = (*env)->GetIsolate();
    for (int i = 0; i < kObjectsCount; i++) {
      objects_[i].Reset(isolate_, v8::Object::New(isolate_));
    }
    (*env)->Global()->Set(v8_str("root_object"),
                          v8::Local<v8::Value>::New(isolate_, objects_[0]));
  }
  ~GraphWithImplicitRefs() {
    instance_ = NULL;
  }

  static void gcPrologue(v8::GCType type, v8::GCCallbackFlags flags) {
    instance_->AddImplicitReferences();
  }

 private:
  void AddImplicitReferences() {
    // 0 -> 1
    isolate_->SetObjectGroupId(objects_[0],
                               v8::UniqueId(1));
    isolate_->SetReferenceFromGroup(
        v8::UniqueId(1), objects_[1]);
    // Adding two more references: 1 -> 2, 1 -> 3
    isolate_->SetReference(objects_[1].As<v8::Object>(),
                           objects_[2]);
    isolate_->SetReference(objects_[1].As<v8::Object>(),
                           objects_[3]);
  }

  v8::Persistent<v8::Value> objects_[kObjectsCount];
  static GraphWithImplicitRefs* instance_;
  v8::Isolate* isolate_;
};

GraphWithImplicitRefs* GraphWithImplicitRefs::instance_ = NULL;


TEST(HeapSnapshotImplicitReferences) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  GraphWithImplicitRefs graph(&env);
  v8::V8::AddGCPrologueCallback(&GraphWithImplicitRefs::gcPrologue);

  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("implicit_refs"));
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global_object = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* obj0 = GetProperty(
      global_object, v8::HeapGraphEdge::kProperty, "root_object");
  CHECK(obj0);
  CHECK_EQ(v8::HeapGraphNode::kObject, obj0->GetType());
  const v8::HeapGraphNode* obj1 = GetProperty(
      obj0, v8::HeapGraphEdge::kInternal, "native");
  CHECK(obj1);
  int implicit_targets_count = 0;
  for (int i = 0, count = obj1->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = obj1->GetChild(i);
    v8::String::Utf8Value prop_name(prop->GetName());
    if (prop->GetType() == v8::HeapGraphEdge::kInternal &&
        strcmp("native", *prop_name) == 0) {
      ++implicit_targets_count;
    }
  }
  CHECK_EQ(2, implicit_targets_count);
  v8::V8::RemoveGCPrologueCallback(&GraphWithImplicitRefs::gcPrologue);
}


TEST(DeleteAllHeapSnapshots) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  heap_profiler->DeleteAllHeapSnapshots();
  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  CHECK_NE(NULL, heap_profiler->TakeHeapSnapshot(v8_str("1")));
  CHECK_EQ(1, heap_profiler->GetSnapshotCount());
  heap_profiler->DeleteAllHeapSnapshots();
  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  CHECK_NE(NULL, heap_profiler->TakeHeapSnapshot(v8_str("1")));
  CHECK_NE(NULL, heap_profiler->TakeHeapSnapshot(v8_str("2")));
  CHECK_EQ(2, heap_profiler->GetSnapshotCount());
  heap_profiler->DeleteAllHeapSnapshots();
  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
}


static const v8::HeapSnapshot* FindHeapSnapshot(v8::HeapProfiler* profiler,
                                                unsigned uid) {
  int length = profiler->GetSnapshotCount();
  for (int i = 0; i < length; i++) {
    const v8::HeapSnapshot* snapshot = profiler->GetHeapSnapshot(i);
    if (snapshot->GetUid() == uid) {
      return snapshot;
    }
  }
  return NULL;
}


TEST(DeleteHeapSnapshot) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  const v8::HeapSnapshot* s1 =
      heap_profiler->TakeHeapSnapshot(v8_str("1"));

  CHECK_NE(NULL, s1);
  CHECK_EQ(1, heap_profiler->GetSnapshotCount());
  unsigned uid1 = s1->GetUid();
  CHECK_EQ(s1, FindHeapSnapshot(heap_profiler, uid1));
  const_cast<v8::HeapSnapshot*>(s1)->Delete();
  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  CHECK_EQ(NULL, FindHeapSnapshot(heap_profiler, uid1));

  const v8::HeapSnapshot* s2 =
      heap_profiler->TakeHeapSnapshot(v8_str("2"));
  CHECK_NE(NULL, s2);
  CHECK_EQ(1, heap_profiler->GetSnapshotCount());
  unsigned uid2 = s2->GetUid();
  CHECK_NE(static_cast<int>(uid1), static_cast<int>(uid2));
  CHECK_EQ(s2, FindHeapSnapshot(heap_profiler, uid2));
  const v8::HeapSnapshot* s3 =
      heap_profiler->TakeHeapSnapshot(v8_str("3"));
  CHECK_NE(NULL, s3);
  CHECK_EQ(2, heap_profiler->GetSnapshotCount());
  unsigned uid3 = s3->GetUid();
  CHECK_NE(static_cast<int>(uid1), static_cast<int>(uid3));
  CHECK_EQ(s3, FindHeapSnapshot(heap_profiler, uid3));
  const_cast<v8::HeapSnapshot*>(s2)->Delete();
  CHECK_EQ(1, heap_profiler->GetSnapshotCount());
  CHECK_EQ(NULL, FindHeapSnapshot(heap_profiler, uid2));
  CHECK_EQ(s3, FindHeapSnapshot(heap_profiler, uid3));
  const_cast<v8::HeapSnapshot*>(s3)->Delete();
  CHECK_EQ(0, heap_profiler->GetSnapshotCount());
  CHECK_EQ(NULL, FindHeapSnapshot(heap_profiler, uid3));
}


class NameResolver : public v8::HeapProfiler::ObjectNameResolver {
 public:
  virtual const char* GetName(v8::Handle<v8::Object> object) {
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
      heap_profiler->TakeHeapSnapshot(v8_str("document"),
      NULL,
      &name_resolver);
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  CHECK_EQ("Object / Global object name" ,
           const_cast<i::HeapEntry*>(
               reinterpret_cast<const i::HeapEntry*>(global))->name());
}


TEST(GlobalObjectFields) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("obj = {};");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* builtins =
      GetProperty(global, v8::HeapGraphEdge::kInternal, "builtins");
  CHECK_NE(NULL, builtins);
  const v8::HeapGraphNode* native_context =
      GetProperty(global, v8::HeapGraphEdge::kInternal, "native_context");
  CHECK_NE(NULL, native_context);
  const v8::HeapGraphNode* global_context =
      GetProperty(global, v8::HeapGraphEdge::kInternal, "global_context");
  CHECK_NE(NULL, global_context);
  const v8::HeapGraphNode* global_proxy =
      GetProperty(global, v8::HeapGraphEdge::kInternal, "global_proxy");
  CHECK_NE(NULL, global_proxy);
}


TEST(NoHandleLeaks) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("document = { URL:\"abcdefgh\" };");

  v8::Handle<v8::String> name(v8_str("leakz"));
  i::Isolate* isolate = CcTest::i_isolate();
  int count_before = i::HandleScope::NumberOfHandles(isolate);
  heap_profiler->TakeHeapSnapshot(name);
  int count_after = i::HandleScope::NumberOfHandles(isolate);
  CHECK_EQ(count_before, count_after);
}


TEST(NodesIteration) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("iteration"));
  CHECK(ValidateSnapshot(snapshot));
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


TEST(GetHeapValueForNode) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("a = { s_prop: \'value\', n_prop: \'value2\' };");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("value"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK(heap_profiler->FindObjectById(global->GetId())->IsObject());
  v8::Local<v8::Object> js_global =
      env->Global()->GetPrototype().As<v8::Object>();
  CHECK(js_global == heap_profiler->FindObjectById(global->GetId()));
  const v8::HeapGraphNode* obj = GetProperty(
      global, v8::HeapGraphEdge::kProperty, "a");
  CHECK(heap_profiler->FindObjectById(obj->GetId())->IsObject());
  v8::Local<v8::Object> js_obj = js_global->Get(v8_str("a")).As<v8::Object>();
  CHECK(js_obj == heap_profiler->FindObjectById(obj->GetId()));
  const v8::HeapGraphNode* s_prop =
      GetProperty(obj, v8::HeapGraphEdge::kProperty, "s_prop");
  v8::Local<v8::String> js_s_prop =
      js_obj->Get(v8_str("s_prop")).As<v8::String>();
  CHECK(js_s_prop == heap_profiler->FindObjectById(s_prop->GetId()));
  const v8::HeapGraphNode* n_prop =
      GetProperty(obj, v8::HeapGraphEdge::kProperty, "n_prop");
  v8::Local<v8::String> js_n_prop =
      js_obj->Get(v8_str("n_prop")).As<v8::String>();
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
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* obj = GetProperty(
      global, v8::HeapGraphEdge::kProperty, "a");
  const v8::HeapGraphNode* prop = GetProperty(
      obj, v8::HeapGraphEdge::kProperty, "p");
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
  i::SmartArrayPointer<char> s_act = act->ToCString();
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
  // TODO(verwaest): Restore to Constructor3 once supported by the
  // heap-snapshot-generator.
  CHECK_EQ(
      0, StringCmp("Object", i::V8HeapExplorer::GetConstructorName(*js_obj3)));
  v8::Local<v8::Object> obj4 = js_global->Get(v8_str("obj4")).As<v8::Object>();
  i::Handle<i::JSObject> js_obj4 = v8::Utils::OpenHandle(*obj4);
  // TODO(verwaest): Restore to Constructor4 once supported by the
  // heap-snapshot-generator.
  CHECK_EQ(
      0, StringCmp("Object", i::V8HeapExplorer::GetConstructorName(*js_obj4)));
  v8::Local<v8::Object> obj5 = js_global->Get(v8_str("obj5")).As<v8::Object>();
  i::Handle<i::JSObject> js_obj5 = v8::Utils::OpenHandle(*obj5);
  CHECK_EQ(0, StringCmp(
      "Object", i::V8HeapExplorer::GetConstructorName(*js_obj5)));
  v8::Local<v8::Object> obj6 = js_global->Get(v8_str("obj6")).As<v8::Object>();
  i::Handle<i::JSObject> js_obj6 = v8::Utils::OpenHandle(*obj6);
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
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("fastCaseAccessors"));
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  const v8::HeapGraphNode* obj1 =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "obj1");
  CHECK_NE(NULL, obj1);
  const v8::HeapGraphNode* func;
  func = GetProperty(obj1, v8::HeapGraphEdge::kProperty, "get propWithGetter");
  CHECK_NE(NULL, func);
  func = GetProperty(obj1, v8::HeapGraphEdge::kProperty, "set propWithGetter");
  CHECK_EQ(NULL, func);
  func = GetProperty(obj1, v8::HeapGraphEdge::kProperty, "set propWithSetter");
  CHECK_NE(NULL, func);
  func = GetProperty(obj1, v8::HeapGraphEdge::kProperty, "get propWithSetter");
  CHECK_EQ(NULL, func);
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
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("slowCaseAccessors"));
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  const v8::HeapGraphNode* obj1 =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "obj1");
  CHECK_NE(NULL, obj1);
  const v8::HeapGraphNode* func;
  func = GetProperty(obj1, v8::HeapGraphEdge::kProperty, "get propWithGetter");
  CHECK_NE(NULL, func);
  func = GetProperty(obj1, v8::HeapGraphEdge::kProperty, "set propWithGetter");
  CHECK_EQ(NULL, func);
  func = GetProperty(obj1, v8::HeapGraphEdge::kProperty, "set propWithSetter");
  CHECK_NE(NULL, func);
  func = GetProperty(obj1, v8::HeapGraphEdge::kProperty, "get propWithSetter");
  CHECK_EQ(NULL, func);
}


TEST(HiddenPropertiesFastCase) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "function C(x) { this.a = this; this.b = x; }\n"
      "c = new C(2012);\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("HiddenPropertiesFastCase1"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* c =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "c");
  CHECK_NE(NULL, c);
  const v8::HeapGraphNode* hidden_props =
      GetProperty(c, v8::HeapGraphEdge::kInternal, "hidden_properties");
  CHECK_EQ(NULL, hidden_props);

  v8::Handle<v8::Value> cHandle =
      env->Global()->Get(v8::String::NewFromUtf8(env->GetIsolate(), "c"));
  CHECK(!cHandle.IsEmpty() && cHandle->IsObject());
  cHandle->ToObject()->SetHiddenValue(v8_str("key"), v8_str("val"));

  snapshot = heap_profiler->TakeHeapSnapshot(
      v8_str("HiddenPropertiesFastCase2"));
  CHECK(ValidateSnapshot(snapshot));
  global = GetGlobalObject(snapshot);
  c = GetProperty(global, v8::HeapGraphEdge::kProperty, "c");
  CHECK_NE(NULL, c);
  hidden_props = GetProperty(c, v8::HeapGraphEdge::kInternal,
      "hidden_properties");
  CHECK_NE(NULL, hidden_props);
}


TEST(AccessorInfo) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("function foo(x) { }\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("AccessorInfoTest"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* foo =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "foo");
  CHECK_NE(NULL, foo);
  const v8::HeapGraphNode* map =
      GetProperty(foo, v8::HeapGraphEdge::kInternal, "map");
  CHECK_NE(NULL, map);
  const v8::HeapGraphNode* descriptors =
      GetProperty(map, v8::HeapGraphEdge::kInternal, "descriptors");
  CHECK_NE(NULL, descriptors);
  const v8::HeapGraphNode* length_name =
      GetProperty(descriptors, v8::HeapGraphEdge::kInternal, "2");
  CHECK_NE(NULL, length_name);
  CHECK_EQ("length", *v8::String::Utf8Value(length_name->GetName()));
  const v8::HeapGraphNode* length_accessor =
      GetProperty(descriptors, v8::HeapGraphEdge::kInternal, "4");
  CHECK_NE(NULL, length_accessor);
  CHECK_EQ("system / ExecutableAccessorInfo",
           *v8::String::Utf8Value(length_accessor->GetName()));
  const v8::HeapGraphNode* name =
      GetProperty(length_accessor, v8::HeapGraphEdge::kInternal, "name");
  CHECK_NE(NULL, name);
  const v8::HeapGraphNode* getter =
      GetProperty(length_accessor, v8::HeapGraphEdge::kInternal, "getter");
  CHECK_NE(NULL, getter);
  const v8::HeapGraphNode* setter =
      GetProperty(length_accessor, v8::HeapGraphEdge::kInternal, "setter");
  CHECK_NE(NULL, setter);
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
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("weaks"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* gc_roots = GetNode(
      snapshot->GetRoot(), v8::HeapGraphNode::kSynthetic, "(GC roots)");
  CHECK_NE(NULL, gc_roots);
  const v8::HeapGraphNode* global_handles = GetNode(
      gc_roots, v8::HeapGraphNode::kSynthetic, "(Global handles)");
  CHECK_NE(NULL, global_handles);
  return HasWeakEdge(global_handles);
}


static void PersistentHandleCallback(
    const v8::WeakCallbackData<v8::Object, v8::Persistent<v8::Object> >& data) {
  data.GetParameter()->Reset();
  delete data.GetParameter();
}


TEST(WeakGlobalHandle) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  CHECK(!HasWeakGlobalHandle());

  v8::Persistent<v8::Object> handle(env->GetIsolate(),
                                    v8::Object::New(env->GetIsolate()));
  handle.SetWeak(&handle, PersistentHandleCallback);

  CHECK(HasWeakGlobalHandle());
}


TEST(SfiAndJsFunctionWeakRefs) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun(
      "fun = (function (x) { return function () { return x + 1; } })(1);");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("fun"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  const v8::HeapGraphNode* fun =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "fun");
  CHECK(!HasWeakEdge(fun));
  const v8::HeapGraphNode* shared =
      GetProperty(fun, v8::HeapGraphEdge::kInternal, "shared");
  CHECK(!HasWeakEdge(shared));
}


TEST(NoDebugObjectInSnapshot) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CHECK(CcTest::i_isolate()->debug()->Load());
  CompileRun("foo = {};");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* root = snapshot->GetRoot();
  int globals_count = 0;
  for (int i = 0; i < root->GetChildrenCount(); ++i) {
    const v8::HeapGraphEdge* edge = root->GetChild(i);
    if (edge->GetType() == v8::HeapGraphEdge::kShortcut) {
      ++globals_count;
      const v8::HeapGraphNode* global = edge->GetToNode();
      const v8::HeapGraphNode* foo =
          GetProperty(global, v8::HeapGraphEdge::kProperty, "foo");
      CHECK_NE(NULL, foo);
    }
  }
  CHECK_EQ(1, globals_count);
}


TEST(AllStrongGcRootsHaveNames) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();

  CompileRun("foo = {};");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* gc_roots = GetNode(
      snapshot->GetRoot(), v8::HeapGraphNode::kSynthetic, "(GC roots)");
  CHECK_NE(NULL, gc_roots);
  const v8::HeapGraphNode* strong_roots = GetNode(
      gc_roots, v8::HeapGraphNode::kSynthetic, "(Strong roots)");
  CHECK_NE(NULL, strong_roots);
  for (int i = 0; i < strong_roots->GetChildrenCount(); ++i) {
    const v8::HeapGraphEdge* edge = strong_roots->GetChild(i);
    CHECK_EQ(v8::HeapGraphEdge::kInternal, edge->GetType());
    v8::String::Utf8Value name(edge->GetName());
    CHECK(isalpha(**name));
  }
}


TEST(NoRefsToNonEssentialEntries) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("global_object = {};\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* global_object =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "global_object");
  CHECK_NE(NULL, global_object);
  const v8::HeapGraphNode* properties =
      GetProperty(global_object, v8::HeapGraphEdge::kInternal, "properties");
  CHECK_EQ(NULL, properties);
  const v8::HeapGraphNode* elements =
      GetProperty(global_object, v8::HeapGraphEdge::kInternal, "elements");
  CHECK_EQ(NULL, elements);
}


TEST(MapHasDescriptorsAndTransitions) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("obj = { a: 10 };\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* global_object =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "obj");
  CHECK_NE(NULL, global_object);

  const v8::HeapGraphNode* map =
      GetProperty(global_object, v8::HeapGraphEdge::kInternal, "map");
  CHECK_NE(NULL, map);
  const v8::HeapGraphNode* own_descriptors = GetProperty(
      map, v8::HeapGraphEdge::kInternal, "descriptors");
  CHECK_NE(NULL, own_descriptors);
  const v8::HeapGraphNode* own_transitions = GetProperty(
      map, v8::HeapGraphEdge::kInternal, "transitions");
  CHECK_EQ(NULL, own_transitions);
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
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  const v8::HeapGraphNode* ok_object =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "ok");
  CHECK_NE(NULL, ok_object);
  const v8::HeapGraphNode* context_object =
      GetProperty(ok_object, v8::HeapGraphEdge::kInternal, "context");
  CHECK_NE(NULL, context_object);
  // Check the objects are not duplicated in the context.
  CHECK_EQ(v8::internal::Context::MIN_CONTEXT_SLOTS + num_objects - 1,
           context_object->GetChildrenCount());
  // Check all the objects have got their names.
  // ... well check just every 15th because otherwise it's too slow in debug.
  for (int i = 0; i < num_objects - 1; i += 15) {
    i::EmbeddedVector<char, 100> var_name;
    i::SNPrintF(var_name, "f_%d", i);
    const v8::HeapGraphNode* f_object = GetProperty(
        context_object, v8::HeapGraphEdge::kContextVariable, var_name.start());
    CHECK_NE(NULL, f_object);
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
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));

  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_NE(NULL, global);
  const v8::HeapGraphNode* fun_code =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "fun");
  CHECK_NE(NULL, fun_code);
  const v8::HeapGraphNode* literals =
      GetProperty(fun_code, v8::HeapGraphEdge::kInternal, "literals");
  CHECK_NE(NULL, literals);
  CHECK_EQ(v8::HeapGraphNode::kArray, literals->GetType());
  CHECK_EQ(2, literals->GetChildrenCount());

  // The second value in the literals array should be the boilerplate,
  // after an AllocationSite.
  const v8::HeapGraphEdge* prop = literals->GetChild(1);
  const v8::HeapGraphNode* allocation_site = prop->GetToNode();
  v8::String::Utf8Value name(allocation_site->GetName());
  CHECK_EQ("system / AllocationSite", *name);
  const v8::HeapGraphNode* transition_info =
      GetProperty(allocation_site, v8::HeapGraphEdge::kInternal,
                  "transition_info");
  CHECK_NE(NULL, transition_info);

  const v8::HeapGraphNode* elements =
      GetProperty(transition_info, v8::HeapGraphEdge::kInternal,
                  "elements");
  CHECK_NE(NULL, elements);
  CHECK_EQ(v8::HeapGraphNode::kArray, elements->GetType());
  CHECK_EQ(v8::internal::FixedArray::SizeFor(3),
           static_cast<int>(elements->GetShallowSize()));

  v8::Handle<v8::Value> array_val =
      heap_profiler->FindObjectById(transition_info->GetId());
  CHECK(array_val->IsArray());
  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(array_val);
  // Verify the array is "a" in the code above.
  CHECK_EQ(3, array->Length());
  CHECK_EQ(v8::Integer::New(isolate, 3),
           array->Get(v8::Integer::New(isolate, 0)));
  CHECK_EQ(v8::Integer::New(isolate, 2),
           array->Get(v8::Integer::New(isolate, 1)));
  CHECK_EQ(v8::Integer::New(isolate, 1),
           array->Get(v8::Integer::New(isolate, 2)));
}


TEST(JSFunctionHasCodeLink) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("function foo(x, y) { return x + y; }\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* foo_func =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "foo");
  CHECK_NE(NULL, foo_func);
  const v8::HeapGraphNode* code =
      GetProperty(foo_func, v8::HeapGraphEdge::kInternal, "code");
  CHECK_NE(NULL, code);
}


static const v8::HeapGraphNode* GetNodeByPath(const v8::HeapSnapshot* snapshot,
                                              const char* path[],
                                              int depth) {
  const v8::HeapGraphNode* node = snapshot->GetRoot();
  for (int current_depth = 0; current_depth < depth; ++current_depth) {
    int i, count = node->GetChildrenCount();
    for (i = 0; i < count; ++i) {
      const v8::HeapGraphEdge* edge = node->GetChild(i);
      const v8::HeapGraphNode* to_node = edge->GetToNode();
      v8::String::Utf8Value edge_name(edge->GetName());
      v8::String::Utf8Value node_name(to_node->GetName());
      i::EmbeddedVector<char, 100> name;
      i::SNPrintF(name, "%s::%s", *edge_name, *node_name);
      if (strstr(name.start(), path[current_depth])) {
        node = to_node;
        break;
      }
    }
    if (i == count) return NULL;
  }
  return node;
}


TEST(CheckCodeNames) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("var a = 1.1;");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("CheckCodeNames"));
  CHECK(ValidateSnapshot(snapshot));

  const char* stub_path[] = {
    "::(GC roots)",
    "::(Strong roots)",
    "code_stubs::",
    "::(ArraySingleArgumentConstructorStub code)"
  };
  const v8::HeapGraphNode* node = GetNodeByPath(snapshot,
      stub_path, arraysize(stub_path));
  CHECK_NE(NULL, node);

  const char* builtin_path1[] = {
    "::(GC roots)",
    "::(Builtins)",
    "::(KeyedLoadIC_Generic builtin)"
  };
  node = GetNodeByPath(snapshot, builtin_path1, arraysize(builtin_path1));
  CHECK_NE(NULL, node);

  const char* builtin_path2[] = {"::(GC roots)", "::(Builtins)",
                                 "::(CompileLazy builtin)"};
  node = GetNodeByPath(snapshot, builtin_path2, arraysize(builtin_path2));
  CHECK_NE(NULL, node);
  v8::String::Utf8Value node_name(node->GetName());
  CHECK_EQ("(CompileLazy builtin)", *node_name);
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
  for (int i = 0; node != NULL && i < names.length(); i++) {
    const char* name = names[i];
    Vector<AllocationTraceNode*> children = node->children();
    node = NULL;
    for (int j = 0; j < children.length(); j++) {
      unsigned index = children[j]->function_info_index();
      AllocationTracker::FunctionInfo* info =
          tracker->function_info_list()[index];
      if (info && strcmp(info->name, name) == 0) {
        node = children[j];
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
  CHECK_NE(NULL, tracker);
  // Resolve all function locations.
  tracker->PrepareForSerialization();
  // Print for better diagnostics in case of failure.
  tracker->trace_tree()->Print(tracker);

  AllocationTraceNode* node =
      FindNode(tracker, Vector<const char*>(names, arraysize(names)));
  CHECK_NE(NULL, node);
  CHECK_GE(node->allocation_count(), 2);
  CHECK_GE(node->allocation_size(), 4 * 5);
  heap_profiler->StopTrackingHeapObjects();
}


TEST(TrackHeapAllocations) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  LocalContext env;

  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  heap_profiler->StartTrackingHeapObjects(true);

  CompileRun(record_trace_tree_source);

  AllocationTracker* tracker =
      reinterpret_cast<i::HeapProfiler*>(heap_profiler)->allocation_tracker();
  CHECK_NE(NULL, tracker);
  // Resolve all function locations.
  tracker->PrepareForSerialization();
  // Print for better diagnostics in case of failure.
  tracker->trace_tree()->Print(tracker);

  const char* names[] = {"", "start", "f_0_0", "f_0_1", "f_0_2"};
  AllocationTraceNode* node =
      FindNode(tracker, Vector<const char*>(names, arraysize(names)));
  CHECK_NE(NULL, node);
  CHECK_GE(node->allocation_count(), 100);
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
    CHECK_NE(NULL, tracker);
    // Resolve all function locations.
    tracker->PrepareForSerialization();
    // Print for better diagnostics in case of failure.
    tracker->trace_tree()->Print(tracker);

    AllocationTraceNode* node =
        FindNode(tracker, Vector<const char*>(names, arraysize(names)));
    CHECK_NE(NULL, node);
    CHECK_GE(node->allocation_count(), 100);
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
    CHECK_NE(NULL, tracker);
    // Resolve all function locations.
    tracker->PrepareForSerialization();
    // Print for better diagnostics in case of failure.
    tracker->trace_tree()->Print(tracker);

    AllocationTraceNode* node =
        FindNode(tracker, Vector<const char*>(names, arraysize(names)));
    CHECK_NE(NULL, node);
    CHECK_LT(node->allocation_count(), 100);

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

  v8::Handle<v8::Object> o1 = v8::Object::New(env->GetIsolate());
  o1->Clone();

  AllocationTracker* tracker =
      reinterpret_cast<i::HeapProfiler*>(heap_profiler)->allocation_tracker();
  CHECK_NE(NULL, tracker);
  // Resolve all function locations.
  tracker->PrepareForSerialization();
  // Print for better diagnostics in case of failure.
  tracker->trace_tree()->Print(tracker);

  AllocationTraceNode* node =
      FindNode(tracker, Vector<const char*>(names, arraysize(names)));
  CHECK_NE(NULL, node);
  CHECK_GE(node->allocation_count(), 2);
  CHECK_GE(node->allocation_size(), 4 * node->allocation_count());
  heap_profiler->StopTrackingHeapObjects();
}


TEST(ArrayBufferAndArrayBufferView) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun("arr1 = new Uint32Array(100);\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* arr1_obj =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "arr1");
  CHECK_NE(NULL, arr1_obj);
  const v8::HeapGraphNode* arr1_buffer =
      GetProperty(arr1_obj, v8::HeapGraphEdge::kInternal, "buffer");
  CHECK_NE(NULL, arr1_buffer);
  const v8::HeapGraphNode* first_view =
      GetProperty(arr1_buffer, v8::HeapGraphEdge::kWeak, "weak_first_view");
  CHECK_NE(NULL, first_view);
  const v8::HeapGraphNode* backing_store =
      GetProperty(arr1_buffer, v8::HeapGraphEdge::kInternal, "backing_store");
  CHECK_NE(NULL, backing_store);
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
  DCHECK(data != NULL);
  v8::Local<v8::ArrayBuffer> ab2 =
      v8::ArrayBuffer::New(isolate, data, ab_contents.ByteLength());
  CHECK(ab2->IsExternal());
  env->Global()->Set(v8_str("ab1"), ab);
  env->Global()->Set(v8_str("ab2"), ab2);

  v8::Handle<v8::Value> result = CompileRun("ab2.byteLength");
  CHECK_EQ(1024, result->Int32Value());

  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* ab1_node =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "ab1");
  CHECK_NE(NULL, ab1_node);
  const v8::HeapGraphNode* ab1_data =
      GetProperty(ab1_node, v8::HeapGraphEdge::kInternal, "backing_store");
  CHECK_NE(NULL, ab1_data);
  const v8::HeapGraphNode* ab2_node =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "ab2");
  CHECK_NE(NULL, ab2_node);
  const v8::HeapGraphNode* ab2_data =
      GetProperty(ab2_node, v8::HeapGraphEdge::kInternal, "backing_store");
  CHECK_NE(NULL, ab2_data);
  CHECK_EQ(ab1_data, ab2_data);
  CHECK_EQ(2, GetRetainersCount(snapshot, ab1_data));
  free(data);
}


TEST(BoxObject) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  v8::Handle<v8::Object> global_proxy = env->Global();
  v8::Handle<v8::Object> global = global_proxy->GetPrototype().As<v8::Object>();

  i::Factory* factory = CcTest::i_isolate()->factory();
  i::Handle<i::String> string = factory->NewStringFromStaticChars("string");
  i::Handle<i::Object> box = factory->NewBox(string);
  global->Set(0, v8::ToApiHandle<v8::Object>(box));

  v8::HeapProfiler* heap_profiler = isolate->GetHeapProfiler();
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global_node = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* box_node =
      GetProperty(global_node, v8::HeapGraphEdge::kElement, "0");
  CHECK_NE(NULL, box_node);
  v8::String::Utf8Value box_node_name(box_node->GetName());
  CHECK_EQ("system / Box", *box_node_name);
  const v8::HeapGraphNode* box_value =
      GetProperty(box_node, v8::HeapGraphEdge::kInternal, "value");
  CHECK_NE(NULL, box_value);
}


TEST(WeakContainers) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  if (!CcTest::i_isolate()->use_crankshaft()) return;
  v8::HeapProfiler* heap_profiler = env->GetIsolate()->GetHeapProfiler();
  CompileRun(
      "function foo(a) { return a.x; }\n"
      "obj = {x : 123};\n"
      "foo(obj);\n"
      "foo(obj);\n"
      "%OptimizeFunctionOnNextCall(foo);\n"
      "foo(obj);\n");
  const v8::HeapSnapshot* snapshot =
      heap_profiler->TakeHeapSnapshot(v8_str("snapshot"));
  CHECK(ValidateSnapshot(snapshot));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* obj =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "obj");
  CHECK_NE(NULL, obj);
  const v8::HeapGraphNode* map =
      GetProperty(obj, v8::HeapGraphEdge::kInternal, "map");
  CHECK_NE(NULL, map);
  const v8::HeapGraphNode* dependent_code =
      GetProperty(map, v8::HeapGraphEdge::kInternal, "dependent_code");
  if (!dependent_code) return;
  int count = dependent_code->GetChildrenCount();
  CHECK_NE(0, count);
  for (int i = 0; i < count; ++i) {
    const v8::HeapGraphEdge* prop = dependent_code->GetChild(i);
    CHECK_EQ(v8::HeapGraphEdge::kWeak, prop->GetType());
  }
}


static inline i::Address ToAddress(int n) {
  return reinterpret_cast<i::Address>(n);
}


TEST(AddressToTraceMap) {
  i::AddressToTraceMap map;

  CHECK_EQ(0, map.GetTraceNodeId(ToAddress(150)));

  // [0x100, 0x200) -> 1
  map.AddRange(ToAddress(0x100), 0x100, 1U);
  CHECK_EQ(0, map.GetTraceNodeId(ToAddress(0x50)));
  CHECK_EQ(1, map.GetTraceNodeId(ToAddress(0x100)));
  CHECK_EQ(1, map.GetTraceNodeId(ToAddress(0x150)));
  CHECK_EQ(0, map.GetTraceNodeId(ToAddress(0x100 + 0x100)));
  CHECK_EQ(1, static_cast<int>(map.size()));

  // [0x100, 0x200) -> 1, [0x200, 0x300) -> 2
  map.AddRange(ToAddress(0x200), 0x100, 2U);
  CHECK_EQ(2, map.GetTraceNodeId(ToAddress(0x2a0)));
  CHECK_EQ(2, static_cast<int>(map.size()));

  // [0x100, 0x180) -> 1, [0x180, 0x280) -> 3, [0x280, 0x300) -> 2
  map.AddRange(ToAddress(0x180), 0x100, 3U);
  CHECK_EQ(1, map.GetTraceNodeId(ToAddress(0x17F)));
  CHECK_EQ(2, map.GetTraceNodeId(ToAddress(0x280)));
  CHECK_EQ(3, map.GetTraceNodeId(ToAddress(0x180)));
  CHECK_EQ(3, static_cast<int>(map.size()));

  // [0x100, 0x180) -> 1, [0x180, 0x280) -> 3, [0x280, 0x300) -> 2,
  // [0x400, 0x500) -> 4
  map.AddRange(ToAddress(0x400), 0x100, 4U);
  CHECK_EQ(1, map.GetTraceNodeId(ToAddress(0x17F)));
  CHECK_EQ(2, map.GetTraceNodeId(ToAddress(0x280)));
  CHECK_EQ(3, map.GetTraceNodeId(ToAddress(0x180)));
  CHECK_EQ(4, map.GetTraceNodeId(ToAddress(0x450)));
  CHECK_EQ(0, map.GetTraceNodeId(ToAddress(0x500)));
  CHECK_EQ(0, map.GetTraceNodeId(ToAddress(0x350)));
  CHECK_EQ(4, static_cast<int>(map.size()));

  // [0x100, 0x180) -> 1, [0x180, 0x200) -> 3, [0x200, 0x600) -> 5
  map.AddRange(ToAddress(0x200), 0x400, 5U);
  CHECK_EQ(5, map.GetTraceNodeId(ToAddress(0x200)));
  CHECK_EQ(5, map.GetTraceNodeId(ToAddress(0x400)));
  CHECK_EQ(3, static_cast<int>(map.size()));

  // [0x100, 0x180) -> 1, [0x180, 0x200) -> 7, [0x200, 0x600) ->5
  map.AddRange(ToAddress(0x180), 0x80, 6U);
  map.AddRange(ToAddress(0x180), 0x80, 7U);
  CHECK_EQ(7, map.GetTraceNodeId(ToAddress(0x180)));
  CHECK_EQ(5, map.GetTraceNodeId(ToAddress(0x200)));
  CHECK_EQ(3, static_cast<int>(map.size()));

  map.Clear();
  CHECK_EQ(0, static_cast<int>(map.size()));
  CHECK_EQ(0, map.GetTraceNodeId(ToAddress(0x400)));
}
