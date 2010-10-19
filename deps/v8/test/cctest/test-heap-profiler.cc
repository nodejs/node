// Copyright 2009 the V8 project authors. All rights reserved.
//
// Tests for heap profiler

#ifdef ENABLE_LOGGING_AND_PROFILING

#include "v8.h"
#include "heap-profiler.h"
#include "snapshot.h"
#include "string-stream.h"
#include "cctest.h"
#include "zone-inl.h"
#include "../include/v8-profiler.h"

namespace i = v8::internal;
using i::ClustersCoarser;
using i::JSObjectsCluster;
using i::JSObjectsRetainerTree;
using i::JSObjectsClusterTree;
using i::RetainerHeapProfile;


namespace {

class ConstructorHeapProfileTestHelper : public i::ConstructorHeapProfile {
 public:
  ConstructorHeapProfileTestHelper()
    : i::ConstructorHeapProfile(),
      f_name_(i::Factory::NewStringFromAscii(i::CStrVector("F"))),
      f_count_(0) {
  }

  void Call(const JSObjectsCluster& cluster,
            const i::NumberAndSizeInfo& number_and_size) {
    if (f_name_->Equals(cluster.constructor())) {
      CHECK_EQ(f_count_, 0);
      f_count_ = number_and_size.number();
      CHECK_GT(f_count_, 0);
    }
  }

  int f_count() { return f_count_; }

 private:
  i::Handle<i::String> f_name_;
  int f_count_;
};

}  // namespace


TEST(ConstructorProfile) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function F() {}  // A constructor\n"
      "var f1 = new F();\n"
      "var f2 = new F();\n");

  ConstructorHeapProfileTestHelper cons_profile;
  i::AssertNoAllocation no_alloc;
  i::HeapIterator iterator;
  for (i::HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next())
    cons_profile.CollectStats(obj);
  CHECK_EQ(0, cons_profile.f_count());
  cons_profile.PrintStats();
  CHECK_EQ(2, cons_profile.f_count());
}


static JSObjectsCluster AddHeapObjectToTree(JSObjectsRetainerTree* tree,
                                            i::String* constructor,
                                            int instance,
                                            JSObjectsCluster* ref1 = NULL,
                                            JSObjectsCluster* ref2 = NULL,
                                            JSObjectsCluster* ref3 = NULL) {
  JSObjectsCluster o(constructor, reinterpret_cast<i::Object*>(instance));
  JSObjectsClusterTree* o_tree = new JSObjectsClusterTree();
  JSObjectsClusterTree::Locator o_loc;
  if (ref1 != NULL) o_tree->Insert(*ref1, &o_loc);
  if (ref2 != NULL) o_tree->Insert(*ref2, &o_loc);
  if (ref3 != NULL) o_tree->Insert(*ref3, &o_loc);
  JSObjectsRetainerTree::Locator loc;
  tree->Insert(o, &loc);
  loc.set_value(o_tree);
  return o;
}


static void AddSelfReferenceToTree(JSObjectsRetainerTree* tree,
                                   JSObjectsCluster* self_ref) {
  JSObjectsRetainerTree::Locator loc;
  CHECK(tree->Find(*self_ref, &loc));
  JSObjectsClusterTree::Locator o_loc;
  CHECK_NE(NULL, loc.value());
  loc.value()->Insert(*self_ref, &o_loc);
}


static inline void CheckEqualsHelper(const char* file, int line,
                                     const char* expected_source,
                                     const JSObjectsCluster& expected,
                                     const char* value_source,
                                     const JSObjectsCluster& value) {
  if (JSObjectsCluster::Compare(expected, value) != 0) {
    i::HeapStringAllocator allocator;
    i::StringStream stream(&allocator);
    stream.Add("#  Expected: ");
    expected.DebugPrint(&stream);
    stream.Add("\n#  Found: ");
    value.DebugPrint(&stream);
    V8_Fatal(file, line, "CHECK_EQ(%s, %s) failed\n%s",
             expected_source, value_source,
             *stream.ToCString());
  }
}


static inline void CheckNonEqualsHelper(const char* file, int line,
                                     const char* expected_source,
                                     const JSObjectsCluster& expected,
                                     const char* value_source,
                                     const JSObjectsCluster& value) {
  if (JSObjectsCluster::Compare(expected, value) == 0) {
    i::HeapStringAllocator allocator;
    i::StringStream stream(&allocator);
    stream.Add("# !Expected: ");
    expected.DebugPrint(&stream);
    stream.Add("\n#  Found: ");
    value.DebugPrint(&stream);
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n%s",
             expected_source, value_source,
             *stream.ToCString());
  }
}


TEST(ClustersCoarserSimple) {
  v8::HandleScope scope;
  LocalContext env;

  i::ZoneScope zn_scope(i::DELETE_ON_EXIT);

  JSObjectsRetainerTree tree;
  JSObjectsCluster function(i::Heap::function_class_symbol());
  JSObjectsCluster a(*i::Factory::NewStringFromAscii(i::CStrVector("A")));
  JSObjectsCluster b(*i::Factory::NewStringFromAscii(i::CStrVector("B")));

  // o1 <- Function
  JSObjectsCluster o1 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x100, &function);
  // o2 <- Function
  JSObjectsCluster o2 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x200, &function);
  // o3 <- A, B
  JSObjectsCluster o3 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x300, &a, &b);
  // o4 <- B, A
  JSObjectsCluster o4 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x400, &b, &a);
  // o5 <- A, B, Function
  JSObjectsCluster o5 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x500,
                          &a, &b, &function);

  ClustersCoarser coarser;
  coarser.Process(&tree);

  CHECK_EQ(coarser.GetCoarseEquivalent(o1), coarser.GetCoarseEquivalent(o2));
  CHECK_EQ(coarser.GetCoarseEquivalent(o3), coarser.GetCoarseEquivalent(o4));
  CHECK_NE(coarser.GetCoarseEquivalent(o1), coarser.GetCoarseEquivalent(o3));
  CHECK_EQ(JSObjectsCluster(), coarser.GetCoarseEquivalent(o5));
}


TEST(ClustersCoarserMultipleConstructors) {
  v8::HandleScope scope;
  LocalContext env;

  i::ZoneScope zn_scope(i::DELETE_ON_EXIT);

  JSObjectsRetainerTree tree;
  JSObjectsCluster function(i::Heap::function_class_symbol());

  // o1 <- Function
  JSObjectsCluster o1 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x100, &function);
  // a1 <- Function
  JSObjectsCluster a1 =
      AddHeapObjectToTree(&tree, i::Heap::Array_symbol(), 0x1000, &function);
  // o2 <- Function
  JSObjectsCluster o2 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x200, &function);
  // a2 <- Function
  JSObjectsCluster a2 =
      AddHeapObjectToTree(&tree, i::Heap::Array_symbol(), 0x2000, &function);

  ClustersCoarser coarser;
  coarser.Process(&tree);

  CHECK_EQ(coarser.GetCoarseEquivalent(o1), coarser.GetCoarseEquivalent(o2));
  CHECK_EQ(coarser.GetCoarseEquivalent(a1), coarser.GetCoarseEquivalent(a2));
}


TEST(ClustersCoarserPathsTraversal) {
  v8::HandleScope scope;
  LocalContext env;

  i::ZoneScope zn_scope(i::DELETE_ON_EXIT);

  JSObjectsRetainerTree tree;

  // On the following graph:
  //
  // p
  //   <- o21 <- o11 <-
  // q                  o
  //   <- o22 <- o12 <-
  // r
  //
  // we expect that coarser will deduce equivalences: p ~ q ~ r,
  // o21 ~ o22, and o11 ~ o12.

  JSObjectsCluster o =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x100);
  JSObjectsCluster o11 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x110, &o);
  JSObjectsCluster o12 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x120, &o);
  JSObjectsCluster o21 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x210, &o11);
  JSObjectsCluster o22 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x220, &o12);
  JSObjectsCluster p =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x300, &o21);
  JSObjectsCluster q =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x310, &o21, &o22);
  JSObjectsCluster r =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x320, &o22);

  ClustersCoarser coarser;
  coarser.Process(&tree);

  CHECK_EQ(JSObjectsCluster(), coarser.GetCoarseEquivalent(o));
  CHECK_NE(JSObjectsCluster(), coarser.GetCoarseEquivalent(o11));
  CHECK_EQ(coarser.GetCoarseEquivalent(o11), coarser.GetCoarseEquivalent(o12));
  CHECK_EQ(coarser.GetCoarseEquivalent(o21), coarser.GetCoarseEquivalent(o22));
  CHECK_NE(coarser.GetCoarseEquivalent(o11), coarser.GetCoarseEquivalent(o21));
  CHECK_NE(JSObjectsCluster(), coarser.GetCoarseEquivalent(p));
  CHECK_EQ(coarser.GetCoarseEquivalent(p), coarser.GetCoarseEquivalent(q));
  CHECK_EQ(coarser.GetCoarseEquivalent(q), coarser.GetCoarseEquivalent(r));
  CHECK_NE(coarser.GetCoarseEquivalent(o11), coarser.GetCoarseEquivalent(p));
  CHECK_NE(coarser.GetCoarseEquivalent(o21), coarser.GetCoarseEquivalent(p));
}


TEST(ClustersCoarserSelf) {
  v8::HandleScope scope;
  LocalContext env;

  i::ZoneScope zn_scope(i::DELETE_ON_EXIT);

  JSObjectsRetainerTree tree;

  // On the following graph:
  //
  // p (self-referencing)
  //          <- o1     <-
  // q (self-referencing)   o
  //          <- o2     <-
  // r (self-referencing)
  //
  // we expect that coarser will deduce equivalences: p ~ q ~ r, o1 ~ o2;

  JSObjectsCluster o =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x100);
  JSObjectsCluster o1 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x110, &o);
  JSObjectsCluster o2 =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x120, &o);
  JSObjectsCluster p =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x300, &o1);
  AddSelfReferenceToTree(&tree, &p);
  JSObjectsCluster q =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x310, &o1, &o2);
  AddSelfReferenceToTree(&tree, &q);
  JSObjectsCluster r =
      AddHeapObjectToTree(&tree, i::Heap::Object_symbol(), 0x320, &o2);
  AddSelfReferenceToTree(&tree, &r);

  ClustersCoarser coarser;
  coarser.Process(&tree);

  CHECK_EQ(JSObjectsCluster(), coarser.GetCoarseEquivalent(o));
  CHECK_NE(JSObjectsCluster(), coarser.GetCoarseEquivalent(o1));
  CHECK_EQ(coarser.GetCoarseEquivalent(o1), coarser.GetCoarseEquivalent(o2));
  CHECK_NE(JSObjectsCluster(), coarser.GetCoarseEquivalent(p));
  CHECK_EQ(coarser.GetCoarseEquivalent(p), coarser.GetCoarseEquivalent(q));
  CHECK_EQ(coarser.GetCoarseEquivalent(q), coarser.GetCoarseEquivalent(r));
  CHECK_NE(coarser.GetCoarseEquivalent(o1), coarser.GetCoarseEquivalent(p));
}


namespace {

class RetainerProfilePrinter : public RetainerHeapProfile::Printer {
 public:
  RetainerProfilePrinter() : stream_(&allocator_), lines_(100) {}

  void PrintRetainers(const JSObjectsCluster& cluster,
                      const i::StringStream& retainers) {
    cluster.Print(&stream_);
    stream_.Add("%s", *(retainers.ToCString()));
    stream_.Put('\0');
  }

  const char* GetRetainers(const char* constructor) {
    FillLines();
    const size_t cons_len = strlen(constructor);
    for (int i = 0; i < lines_.length(); ++i) {
      if (strncmp(constructor, lines_[i], cons_len) == 0 &&
          lines_[i][cons_len] == ',') {
        return lines_[i] + cons_len + 1;
      }
    }
    return NULL;
  }

 private:
  void FillLines() {
    if (lines_.length() > 0) return;
    stream_.Put('\0');
    stream_str_ = stream_.ToCString();
    const char* pos = *stream_str_;
    while (pos != NULL && *pos != '\0') {
      lines_.Add(pos);
      pos = strchr(pos, '\0');
      if (pos != NULL) ++pos;
    }
  }

  i::HeapStringAllocator allocator_;
  i::StringStream stream_;
  i::SmartPointer<const char> stream_str_;
  i::List<const char*> lines_;
};

}  // namespace


TEST(RetainerProfile) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function A() {}\n"
      "function B(x) { this.x = x; }\n"
      "function C(x) { this.x1 = x; this.x2 = x; }\n"
      "var a = new A();\n"
      "var b1 = new B(a), b2 = new B(a);\n"
      "var c = new C(a);");

  RetainerHeapProfile ret_profile;
  i::AssertNoAllocation no_alloc;
  i::HeapIterator iterator;
  for (i::HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next())
    ret_profile.CollectStats(obj);
  ret_profile.CoarseAndAggregate();
  RetainerProfilePrinter printer;
  ret_profile.DebugPrintStats(&printer);
  const char* retainers_of_a = printer.GetRetainers("A");
  // The order of retainers is unspecified, so we check string length, and
  // verify each retainer separately.
  CHECK_EQ(i::StrLength("(global property);1,B;2,C;2"),
           i::StrLength(retainers_of_a));
  CHECK(strstr(retainers_of_a, "(global property);1") != NULL);
  CHECK(strstr(retainers_of_a, "B;2") != NULL);
  CHECK(strstr(retainers_of_a, "C;2") != NULL);
  CHECK_EQ("(global property);2", printer.GetRetainers("B"));
  CHECK_EQ("(global property);1", printer.GetRetainers("C"));
}


namespace {

class NamedEntriesDetector {
 public:
  NamedEntriesDetector()
      : has_A1(false), has_B1(false), has_C1(false),
        has_A2(false), has_B2(false), has_C2(false) {
  }

  void Apply(i::HeapEntry** entry_ptr) {
    if (IsReachableNodeWithName(*entry_ptr, "A1")) has_A1 = true;
    if (IsReachableNodeWithName(*entry_ptr, "B1")) has_B1 = true;
    if (IsReachableNodeWithName(*entry_ptr, "C1")) has_C1 = true;
    if (IsReachableNodeWithName(*entry_ptr, "A2")) has_A2 = true;
    if (IsReachableNodeWithName(*entry_ptr, "B2")) has_B2 = true;
    if (IsReachableNodeWithName(*entry_ptr, "C2")) has_C2 = true;
  }

  static bool IsReachableNodeWithName(i::HeapEntry* entry, const char* name) {
    return strcmp(name, entry->name()) == 0 && entry->painted_reachable();
  }

  bool has_A1;
  bool has_B1;
  bool has_C1;
  bool has_A2;
  bool has_B2;
  bool has_C2;
};

}  // namespace


static const v8::HeapGraphNode* GetGlobalObject(
    const v8::HeapSnapshot* snapshot) {
  CHECK_EQ(1, snapshot->GetRoot()->GetChildrenCount());
  return snapshot->GetRoot()->GetChild(0)->GetToNode();
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


static bool IsNodeRetainedAs(const v8::HeapGraphNode* node,
                             v8::HeapGraphEdge::Type type,
                             const char* name) {
  for (int i = 0, count = node->GetRetainersCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = node->GetRetainer(i);
    v8::String::AsciiValue prop_name(prop->GetName());
    if (prop->GetType() == type && strcmp(name, *prop_name) == 0)
      return true;
  }
  return false;
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
  v8::Handle<v8::String> token1 = v8::String::New("token1");
  LocalContext env1;
  env1->SetSecurityToken(token1);

  CompileRun(
      "function A1() {}\n"
      "function B1(x) { this.x = x; }\n"
      "function C1(x) { this.x1 = x; this.x2 = x; }\n"
      "var a1 = new A1();\n"
      "var b1_1 = new B1(a1), b1_2 = new B1(a1);\n"
      "var c1 = new C1(a1);");

  v8::Handle<v8::String> token2 = v8::String::New("token2");
  LocalContext env2;
  env2->SetSecurityToken(token2);

  CompileRun(
      "function A2() {}\n"
      "function B2(x) { return function() { return typeof x; }; }\n"
      "function C2(x) { this.x1 = x; this.x2 = x; this[1] = x; }\n"
      "var a2 = new A2();\n"
      "var b2_1 = new B2(a2), b2_2 = new B2(a2);\n"
      "var c2 = new C2(a2);");
  const v8::HeapSnapshot* snapshot_env2 =
      v8::HeapProfiler::TakeSnapshot(v8::String::New("env2"));
  i::HeapSnapshot* i_snapshot_env2 =
      const_cast<i::HeapSnapshot*>(
          reinterpret_cast<const i::HeapSnapshot*>(snapshot_env2));
  const v8::HeapGraphNode* global_env2 = GetGlobalObject(snapshot_env2);
  // Paint all nodes reachable from global object.
  i_snapshot_env2->ClearPaint();
  const_cast<i::HeapEntry*>(
      reinterpret_cast<const i::HeapEntry*>(global_env2))->PaintAllReachable();

  // Verify, that JS global object of env2 doesn't have '..1'
  // properties, but has '..2' properties.
  CHECK_EQ(NULL, GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "a1"));
  CHECK_EQ(
      NULL, GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "b1_1"));
  CHECK_EQ(
      NULL, GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "b1_2"));
  CHECK_EQ(NULL, GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "c1"));
  const v8::HeapGraphNode* a2_node =
      GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "a2");
  CHECK_NE(NULL, a2_node);
  CHECK_NE(
      NULL, GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "b2_1"));
  CHECK_NE(
      NULL, GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "b2_2"));
  CHECK_NE(NULL, GetProperty(global_env2, v8::HeapGraphEdge::kProperty, "c2"));

  // Verify that anything related to '[ABC]1' is not reachable.
  NamedEntriesDetector det;
  i_snapshot_env2->IterateEntries(&det);
  CHECK(!det.has_A1);
  CHECK(!det.has_B1);
  CHECK(!det.has_C1);
  CHECK(det.has_A2);
  CHECK(det.has_B2);
  CHECK(det.has_C2);

  // Verify 'a2' object retainers. They are:
  //  - (global object).a2
  //  - c2.x1, c2.x2, c2[1]
  //  - b2_1 and b2_2 closures: via 'x' variable
  CHECK_EQ(6, a2_node->GetRetainingPathsCount());
  bool has_global_obj_a2_ref = false;
  bool has_c2_x1_ref = false, has_c2_x2_ref = false, has_c2_1_ref = false;
  bool has_b2_1_x_ref = false, has_b2_2_x_ref = false;
  for (int i = 0; i < a2_node->GetRetainingPathsCount(); ++i) {
    const v8::HeapGraphPath* path = a2_node->GetRetainingPath(i);
    const int edges_count = path->GetEdgesCount();
    CHECK_GT(edges_count, 0);
    const v8::HeapGraphEdge* last_edge = path->GetEdge(edges_count - 1);
    v8::String::AsciiValue last_edge_name(last_edge->GetName());
    if (strcmp("a2", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::kProperty) {
      has_global_obj_a2_ref = true;
      continue;
    }
    CHECK_GT(edges_count, 1);
    const v8::HeapGraphEdge* prev_edge = path->GetEdge(edges_count - 2);
    v8::String::AsciiValue prev_edge_name(prev_edge->GetName());
    if (strcmp("x1", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::kProperty
        && strcmp("c2", *prev_edge_name) == 0) has_c2_x1_ref = true;
    if (strcmp("x2", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::kProperty
        && strcmp("c2", *prev_edge_name) == 0) has_c2_x2_ref = true;
    if (strcmp("1", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::kElement
        && strcmp("c2", *prev_edge_name) == 0) has_c2_1_ref = true;
    if (strcmp("x", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::kContextVariable
        && strcmp("b2_1", *prev_edge_name) == 0) has_b2_1_x_ref = true;
    if (strcmp("x", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::kContextVariable
        && strcmp("b2_2", *prev_edge_name) == 0) has_b2_2_x_ref = true;
  }
  CHECK(has_global_obj_a2_ref);
  CHECK(has_c2_x1_ref);
  CHECK(has_c2_x2_ref);
  CHECK(has_c2_1_ref);
  CHECK(has_b2_1_x_ref);
  CHECK(has_b2_2_x_ref);
}


TEST(HeapSnapshotObjectSizes) {
  v8::HandleScope scope;
  LocalContext env;

  //   -a-> X1 --a
  // x -b-> X2 <-|
  CompileRun(
      "function X(a, b) { this.a = a; this.b = b; }\n"
      "x = new X(new X(), new X());\n"
      "x.a.a = x.b;");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8::String::New("sizes"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  const v8::HeapGraphNode* x =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "x");
  CHECK_NE(NULL, x);
  const v8::HeapGraphNode* x_prototype =
      GetProperty(x, v8::HeapGraphEdge::kProperty, "__proto__");
  CHECK_NE(NULL, x_prototype);
  const v8::HeapGraphNode* x1 =
      GetProperty(x, v8::HeapGraphEdge::kProperty, "a");
  CHECK_NE(NULL, x1);
  const v8::HeapGraphNode* x2 =
      GetProperty(x, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, x2);
  CHECK_EQ(
      x->GetSelfSize() * 3,
      x->GetReachableSize() - x_prototype->GetReachableSize());
  CHECK_EQ(
      x->GetSelfSize() * 3, x->GetRetainedSize());
  CHECK_EQ(
      x1->GetSelfSize() * 2,
      x1->GetReachableSize() - x_prototype->GetReachableSize());
  CHECK_EQ(
      x1->GetSelfSize(), x1->GetRetainedSize());
  CHECK_EQ(
      x2->GetSelfSize(),
      x2->GetReachableSize() - x_prototype->GetReachableSize());
  CHECK_EQ(
      x2->GetSelfSize(), x2->GetRetainedSize());
}


TEST(HeapSnapshotEntryChildren) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function A() { }\n"
      "a = new A;");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8::String::New("children"));
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
      v8::HeapProfiler::TakeSnapshot(v8::String::New("code"));

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
  v8::String::AsciiValue anonymous_name(anonymous->GetName());
  CHECK_EQ("", *anonymous_name);

  // Find references to code.
  const v8::HeapGraphNode* compiled_code =
      GetProperty(compiled, v8::HeapGraphEdge::kInternal, "code");
  CHECK_NE(NULL, compiled_code);
  const v8::HeapGraphNode* lazy_code =
      GetProperty(lazy, v8::HeapGraphEdge::kInternal, "code");
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
      v8::HeapProfiler::TakeSnapshot(v8::String::New("numbers"));
  const v8::HeapGraphNode* global = GetGlobalObject(snapshot);
  CHECK_EQ(NULL, GetProperty(global, v8::HeapGraphEdge::kProperty, "a"));
  const v8::HeapGraphNode* b =
      GetProperty(global, v8::HeapGraphEdge::kProperty, "b");
  CHECK_NE(NULL, b);
  CHECK_EQ(v8::HeapGraphNode::kHeapNumber, b->GetType());
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
      v8::HeapProfiler::TakeSnapshot(v8::String::New("internals"));
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

TEST(HeapEntryIdsAndGC) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function A() {}\n"
      "function B(x) { this.x = x; }\n"
      "var a = new A();\n"
      "var b = new B(a);");
  const v8::HeapSnapshot* snapshot1 =
      v8::HeapProfiler::TakeSnapshot(v8::String::New("s1"));

  i::Heap::CollectAllGarbage(true);  // Enforce compaction.

  const v8::HeapSnapshot* snapshot2 =
      v8::HeapProfiler::TakeSnapshot(v8::String::New("s2"));

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


TEST(HeapSnapshotsDiff) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function A() {}\n"
      "function B(x) { this.x = x; }\n"
      "function A2(a) { for (var i = 0; i < a; ++i) this[i] = i; }\n"
      "var a = new A();\n"
      "var b = new B(a);");
  const v8::HeapSnapshot* snapshot1 =
      v8::HeapProfiler::TakeSnapshot(v8::String::New("s1"));

  CompileRun(
      "delete a;\n"
      "b.x = null;\n"
      "var a = new A2(20);\n"
      "var b2 = new B(a);");
  const v8::HeapSnapshot* snapshot2 =
      v8::HeapProfiler::TakeSnapshot(v8::String::New("s2"));

  const v8::HeapSnapshotsDiff* diff = snapshot1->CompareWith(snapshot2);

  // Verify additions: ensure that addition of A and B was detected.
  const v8::HeapGraphNode* additions_root = diff->GetAdditionsRoot();
  bool found_A = false, found_B = false;
  uint64_t s1_A_id = 0;
  for (int i = 0, count = additions_root->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = additions_root->GetChild(i);
    const v8::HeapGraphNode* node = prop->GetToNode();
    if (node->GetType() == v8::HeapGraphNode::kObject) {
      v8::String::AsciiValue node_name(node->GetName());
      if (strcmp(*node_name, "A2") == 0) {
        CHECK(IsNodeRetainedAs(node, v8::HeapGraphEdge::kProperty, "a"));
        CHECK(!found_A);
        found_A = true;
        s1_A_id = node->GetId();
      } else if (strcmp(*node_name, "B") == 0) {
        CHECK(IsNodeRetainedAs(node, v8::HeapGraphEdge::kProperty, "b2"));
        CHECK(!found_B);
        found_B = true;
      }
    }
  }
  CHECK(found_A);
  CHECK(found_B);

  // Verify deletions: ensure that deletion of A was detected.
  const v8::HeapGraphNode* deletions_root = diff->GetDeletionsRoot();
  bool found_A_del = false;
  uint64_t s2_A_id = 0;
  for (int i = 0, count = deletions_root->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = deletions_root->GetChild(i);
    const v8::HeapGraphNode* node = prop->GetToNode();
    if (node->GetType() == v8::HeapGraphNode::kObject) {
      v8::String::AsciiValue node_name(node->GetName());
      if (strcmp(*node_name, "A") == 0) {
        CHECK(IsNodeRetainedAs(node, v8::HeapGraphEdge::kProperty, "a"));
        CHECK(!found_A_del);
        found_A_del = true;
        s2_A_id = node->GetId();
      }
    }
  }
  CHECK(found_A_del);
  CHECK_NE_UINT64_T(0, s1_A_id);
  CHECK(s1_A_id != s2_A_id);
}


TEST(HeapSnapshotRootPreservedAfterSorting) {
  v8::HandleScope scope;
  LocalContext env;
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(v8::String::New("s"));
  const v8::HeapGraphNode* root1 = snapshot->GetRoot();
  const_cast<i::HeapSnapshot*>(reinterpret_cast<const i::HeapSnapshot*>(
      snapshot))->GetSortedEntriesList();
  const v8::HeapGraphNode* root2 = snapshot->GetRoot();
  CHECK_EQ(root1, root2);
}


namespace v8 {
namespace internal {

class HeapSnapshotTester {
 public:
  static int CalculateNetworkSize(JSObject* obj) {
    return HeapSnapshot::CalculateNetworkSize(obj);
  }
};

} }  // namespace v8::internal

// http://code.google.com/p/v8/issues/detail?id=822
// Trying to call CalculateNetworkSize on an object with elements set
// to non-FixedArray may cause an assertion error in debug builds.
TEST(Issue822) {
  v8::HandleScope scope;
  LocalContext context;
  const int kElementCount = 260;
  uint8_t* pixel_data = reinterpret_cast<uint8_t*>(malloc(kElementCount));
  i::Handle<i::PixelArray> pixels = i::Factory::NewPixelArray(kElementCount,
                                                              pixel_data);
  v8::Handle<v8::Object> obj = v8::Object::New();
  // Set the elements to be the pixels.
  obj->SetIndexedPropertiesToPixelData(pixel_data, kElementCount);
  i::Handle<i::JSObject> jsobj = v8::Utils::OpenHandle(*obj);
  // This call must not cause an assertion error in debug builds.
  i::HeapSnapshotTester::CalculateNetworkSize(*jsobj);
}


static const v8::HeapGraphNode* GetChild(
    const v8::HeapGraphNode* node,
    v8::HeapGraphNode::Type type,
    const char* name,
    const v8::HeapGraphNode* after = NULL) {
  bool ignore_child = after == NULL ? false : true;
  for (int i = 0, count = node->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = node->GetChild(i);
    const v8::HeapGraphNode* child = prop->GetToNode();
    v8::String::AsciiValue child_name(child->GetName());
    if (!ignore_child
        && child->GetType() == type
        && strcmp(name, *child_name) == 0)
      return child;
    if (after != NULL && child == after) ignore_child = false;
  }
  return NULL;
}

static bool IsNodeRetainedAs(const v8::HeapGraphNode* node,
                             int element) {
  for (int i = 0, count = node->GetRetainersCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = node->GetRetainer(i);
    if (prop->GetType() == v8::HeapGraphEdge::kElement
        && element == prop->GetName()->Int32Value())
      return true;
  }
  return false;
}

TEST(AggregatedHeapSnapshot) {
  v8::HandleScope scope;
  LocalContext env;

  CompileRun(
      "function A() {}\n"
      "function B(x) { this.x = x; }\n"
      "var a = new A();\n"
      "var b = new B(a);");
  const v8::HeapSnapshot* snapshot =
      v8::HeapProfiler::TakeSnapshot(
          v8::String::New("agg"), v8::HeapSnapshot::kAggregated);
  const v8::HeapGraphNode* strings = GetChild(snapshot->GetRoot(),
                                              v8::HeapGraphNode::kInternal,
                                              "STRING_TYPE");
  CHECK_NE(NULL, strings);
  CHECK_NE(0, strings->GetSelfSize());
  CHECK_NE(0, strings->GetInstancesCount());
  const v8::HeapGraphNode* maps = GetChild(snapshot->GetRoot(),
                                           v8::HeapGraphNode::kInternal,
                                           "MAP_TYPE");
  CHECK_NE(NULL, maps);
  CHECK_NE(0, maps->GetSelfSize());
  CHECK_NE(0, maps->GetInstancesCount());

  const v8::HeapGraphNode* a = GetChild(snapshot->GetRoot(),
                                        v8::HeapGraphNode::kObject,
                                        "A");
  CHECK_NE(NULL, a);
  CHECK_NE(0, a->GetSelfSize());
  CHECK_EQ(1, a->GetInstancesCount());

  const v8::HeapGraphNode* b = GetChild(snapshot->GetRoot(),
                                        v8::HeapGraphNode::kObject,
                                        "B");
  CHECK_NE(NULL, b);
  CHECK_NE(0, b->GetSelfSize());
  CHECK_EQ(1, b->GetInstancesCount());

  const v8::HeapGraphNode* glob_prop = GetChild(snapshot->GetRoot(),
                                                v8::HeapGraphNode::kObject,
                                                "(global property)",
                                                b);
  CHECK_NE(NULL, glob_prop);
  CHECK_EQ(0, glob_prop->GetSelfSize());
  CHECK_EQ(0, glob_prop->GetInstancesCount());
  CHECK_NE(0, glob_prop->GetChildrenCount());

  const v8::HeapGraphNode* a_from_glob_prop = GetChild(
      glob_prop,
      v8::HeapGraphNode::kObject,
      "A");
  CHECK_NE(NULL, a_from_glob_prop);
  CHECK_EQ(0, a_from_glob_prop->GetSelfSize());
  CHECK_EQ(0, a_from_glob_prop->GetInstancesCount());
  CHECK_EQ(0, a_from_glob_prop->GetChildrenCount());  // Retains nothing.
  CHECK(IsNodeRetainedAs(a_from_glob_prop, 1));  // (global propery) has 1 ref.

  const v8::HeapGraphNode* b_with_children = GetChild(
      snapshot->GetRoot(),
      v8::HeapGraphNode::kObject,
      "B",
      b);
  CHECK_NE(NULL, b_with_children);
  CHECK_EQ(0, b_with_children->GetSelfSize());
  CHECK_EQ(0, b_with_children->GetInstancesCount());
  CHECK_NE(0, b_with_children->GetChildrenCount());

  const v8::HeapGraphNode* a_from_b = GetChild(
      b_with_children,
      v8::HeapGraphNode::kObject,
      "A");
  CHECK_NE(NULL, a_from_b);
  CHECK_EQ(0, a_from_b->GetSelfSize());
  CHECK_EQ(0, a_from_b->GetInstancesCount());
  CHECK_EQ(0, a_from_b->GetChildrenCount());  // Retains nothing.
  CHECK(IsNodeRetainedAs(a_from_b, 1));  // B has 1 ref to A.
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
      v8::HeapProfiler::TakeSnapshot(v8::String::New("json"));
  TestJSONStream stream;
  snapshot->Serialize(&stream, v8::HeapSnapshot::kJSON);
  CHECK_GT(stream.size(), 0);
  CHECK_EQ(1, stream.eos_signaled());
  i::ScopedVector<char> json(stream.size());
  stream.WriteTo(json);

  // Verify that snapshot string is valid JSON.
  AsciiResource json_res(json);
  v8::Local<v8::String> json_string = v8::String::NewExternal(&json_res);
  env->Global()->Set(v8::String::New("json_snapshot"), json_string);
  v8::Local<v8::Value> snapshot_parse_result = CompileRun(
      "var parsed = JSON.parse(json_snapshot); true;");
  CHECK(!snapshot_parse_result.IsEmpty());

  // Verify that snapshot object has required fields.
  v8::Local<v8::Object> parsed_snapshot =
      env->Global()->Get(v8::String::New("parsed"))->ToObject();
  CHECK(parsed_snapshot->Has(v8::String::New("snapshot")));
  CHECK(parsed_snapshot->Has(v8::String::New("nodes")));
  CHECK(parsed_snapshot->Has(v8::String::New("strings")));

  // Verify that nodes meta-info is valid JSON.
  v8::Local<v8::Value> nodes_meta_parse_result = CompileRun(
      "var parsed_meta = JSON.parse(parsed.nodes[0]); true;");
  CHECK(!nodes_meta_parse_result.IsEmpty());

  // Get node and edge "member" offsets.
  v8::Local<v8::Value> meta_analysis_result = CompileRun(
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
      "    children_meta.types[child_type_offset].indexOf('property');");
  CHECK(!meta_analysis_result.IsEmpty());

  // A helper function for processing encoded nodes.
  CompileRun(
      "function GetChildPosByProperty(pos, prop_name) {\n"
      "  var nodes = parsed.nodes;\n"
      "  var strings = parsed.strings;\n"
      "  for (var i = 0,\n"
      "      count = nodes[pos + children_count_offset] * child_fields_count;\n"
      "      i < count; i += child_fields_count) {\n"
      "    var child_pos = pos + children_offset + i;\n"
      "    if (nodes[child_pos + child_type_offset] === property_type\n"
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
      "      parsed.nodes[1 + children_offset + child_to_node_offset],\"b\"),\n"
      "    \"x\"),"
      "  \"s\")");
  CHECK(!string_obj_pos_val.IsEmpty());
  int string_obj_pos =
      static_cast<int>(string_obj_pos_val->ToNumber()->Value());
  v8::Local<v8::Object> nodes_array =
      parsed_snapshot->Get(v8::String::New("nodes"))->ToObject();
  int string_index = static_cast<int>(
      nodes_array->Get(string_obj_pos + 1)->ToNumber()->Value());
  CHECK_GT(string_index, 0);
  v8::Local<v8::Object> strings_array =
      parsed_snapshot->Get(v8::String::New("strings"))->ToObject();
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
      v8::HeapProfiler::TakeSnapshot(v8::String::New("abort"));
  TestJSONStream stream(5);
  snapshot->Serialize(&stream, v8::HeapSnapshot::kJSON);
  CHECK_GT(stream.size(), 0);
  CHECK_EQ(0, stream.eos_signaled());
}

#endif  // ENABLE_LOGGING_AND_PROFILING
