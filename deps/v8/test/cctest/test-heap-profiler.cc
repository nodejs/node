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


static void CompileAndRunScript(const char *src) {
  v8::Script::Compile(v8::String::New(src))->Run();
}


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
  v8::Handle<v8::Context> env = v8::Context::New();
  env->Enter();

  CompileAndRunScript(
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
  v8::Handle<v8::Context> env = v8::Context::New();
  env->Enter();

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
  v8::Handle<v8::Context> env = v8::Context::New();
  env->Enter();

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
  v8::Handle<v8::Context> env = v8::Context::New();
  env->Enter();

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
  v8::Handle<v8::Context> env = v8::Context::New();
  env->Enter();

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
  v8::Handle<v8::Context> env = v8::Context::New();
  env->Enter();

  CompileAndRunScript(
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

  void Apply(i::HeapEntry* entry) {
    const char* node_name = entry->name();
    if (strcmp("A1", node_name) == 0
        && entry->GetRetainingPaths()->length() > 0) has_A1 = true;
    if (strcmp("B1", node_name) == 0
        && entry->GetRetainingPaths()->length() > 0) has_B1 = true;
    if (strcmp("C1", node_name) == 0
        && entry->GetRetainingPaths()->length() > 0) has_C1 = true;
    if (strcmp("A2", node_name) == 0
        && entry->GetRetainingPaths()->length() > 0) has_A2 = true;
    if (strcmp("B2", node_name) == 0
        && entry->GetRetainingPaths()->length() > 0) has_B2 = true;
    if (strcmp("C2", node_name) == 0
        && entry->GetRetainingPaths()->length() > 0) has_C2 = true;
  }

  bool has_A1;
  bool has_B1;
  bool has_C1;
  bool has_A2;
  bool has_B2;
  bool has_C2;
};

}  // namespace

TEST(HeapSnapshot) {
  v8::HandleScope scope;

  v8::Handle<v8::String> token1 = v8::String::New("token1");
  v8::Handle<v8::Context> env1 = v8::Context::New();
  env1->SetSecurityToken(token1);
  env1->Enter();

  CompileAndRunScript(
      "function A1() {}\n"
      "function B1(x) { this.x = x; }\n"
      "function C1(x) { this.x1 = x; this.x2 = x; }\n"
      "var a1 = new A1();\n"
      "var b1_1 = new B1(a1), b1_2 = new B1(a1);\n"
      "var c1 = new C1(a1);");

  v8::Handle<v8::String> token2 = v8::String::New("token2");
  v8::Handle<v8::Context> env2 = v8::Context::New();
  env2->SetSecurityToken(token2);
  env2->Enter();

  CompileAndRunScript(
      "function A2() {}\n"
      "function B2(x) { return function() { return typeof x; }; }\n"
      "function C2(x) { this.x1 = x; this.x2 = x; this[1] = x; }\n"
      "var a2 = new A2();\n"
      "var b2_1 = new B2(a2), b2_2 = new B2(a2);\n"
      "var c2 = new C2(a2);");
  const v8::HeapSnapshot* snapshot_env2 =
      v8::HeapProfiler::TakeSnapshot(v8::String::New("env2"));
  const v8::HeapGraphNode* global_env2;
  if (i::Snapshot::IsEnabled()) {
    // In case if snapshots are enabled, there will present a
    // vanilla deserealized global object, without properties
    // added by the test code.
    CHECK_EQ(2, snapshot_env2->GetHead()->GetChildrenCount());
    // Choose the global object of a bigger size.
    const v8::HeapGraphNode* node0 =
        snapshot_env2->GetHead()->GetChild(0)->GetToNode();
    const v8::HeapGraphNode* node1 =
        snapshot_env2->GetHead()->GetChild(1)->GetToNode();
    global_env2 = node0->GetTotalSize() > node1->GetTotalSize() ?
        node0 : node1;
  } else {
    CHECK_EQ(1, snapshot_env2->GetHead()->GetChildrenCount());
    global_env2 = snapshot_env2->GetHead()->GetChild(0)->GetToNode();
  }

  // Verify, that JS global object of env2 doesn't have '..1'
  // properties, but has '..2' properties.
  bool has_a1 = false, has_b1_1 = false, has_b1_2 = false, has_c1 = false;
  bool has_a2 = false, has_b2_1 = false, has_b2_2 = false, has_c2 = false;
  // This will be needed further.
  const v8::HeapGraphNode* a2_node = NULL;
  for (int i = 0, count = global_env2->GetChildrenCount(); i < count; ++i) {
    const v8::HeapGraphEdge* prop = global_env2->GetChild(i);
    v8::String::AsciiValue prop_name(prop->GetName());
    if (strcmp("a1", *prop_name) == 0) has_a1 = true;
    if (strcmp("b1_1", *prop_name) == 0) has_b1_1 = true;
    if (strcmp("b1_2", *prop_name) == 0) has_b1_2 = true;
    if (strcmp("c1", *prop_name) == 0) has_c1 = true;
    if (strcmp("a2", *prop_name) == 0) {
      has_a2 = true;
      a2_node = prop->GetToNode();
    }
    if (strcmp("b2_1", *prop_name) == 0) has_b2_1 = true;
    if (strcmp("b2_2", *prop_name) == 0) has_b2_2 = true;
    if (strcmp("c2", *prop_name) == 0) has_c2 = true;
  }
  CHECK(!has_a1);
  CHECK(!has_b1_1);
  CHECK(!has_b1_2);
  CHECK(!has_c1);
  CHECK(has_a2);
  CHECK(has_b2_1);
  CHECK(has_b2_2);
  CHECK(has_c2);

  // Verify that anything related to '[ABC]1' is not reachable.
  NamedEntriesDetector det;
  i::HeapSnapshot* i_snapshot_env2 =
      const_cast<i::HeapSnapshot*>(
          reinterpret_cast<const i::HeapSnapshot*>(snapshot_env2));
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
        && last_edge->GetType() == v8::HeapGraphEdge::PROPERTY) {
      has_global_obj_a2_ref = true;
      continue;
    }
    CHECK_GT(edges_count, 1);
    const v8::HeapGraphEdge* prev_edge = path->GetEdge(edges_count - 2);
    v8::String::AsciiValue prev_edge_name(prev_edge->GetName());
    if (strcmp("x1", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::PROPERTY
        && strcmp("c2", *prev_edge_name) == 0) has_c2_x1_ref = true;
    if (strcmp("x2", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::PROPERTY
        && strcmp("c2", *prev_edge_name) == 0) has_c2_x2_ref = true;
    if (strcmp("1", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::ELEMENT
        && strcmp("c2", *prev_edge_name) == 0) has_c2_1_ref = true;
    if (strcmp("x", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::CONTEXT_VARIABLE
        && strcmp("b2_1", *prev_edge_name) == 0) has_b2_1_x_ref = true;
    if (strcmp("x", *last_edge_name) == 0
        && last_edge->GetType() == v8::HeapGraphEdge::CONTEXT_VARIABLE
        && strcmp("b2_2", *prev_edge_name) == 0) has_b2_2_x_ref = true;
  }
  CHECK(has_global_obj_a2_ref);
  CHECK(has_c2_x1_ref);
  CHECK(has_c2_x2_ref);
  CHECK(has_c2_1_ref);
  CHECK(has_b2_1_x_ref);
  CHECK(has_b2_2_x_ref);
}

#endif  // ENABLE_LOGGING_AND_PROFILING
