// Copyright 2009 the V8 project authors. All rights reserved.
//
// Tests for heap profiler

#ifdef ENABLE_LOGGING_AND_PROFILING

#include "v8.h"
#include "heap-profiler.h"
#include "string-stream.h"
#include "cctest.h"

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
  while (iterator.has_next()) {
    i::HeapObject* obj = iterator.next();
    cons_profile.CollectStats(obj);
  }
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
  while (iterator.has_next()) {
    i::HeapObject* obj = iterator.next();
    ret_profile.CollectStats(obj);
  }
  RetainerProfilePrinter printer;
  ret_profile.DebugPrintStats(&printer);
  const char* retainers_of_a = printer.GetRetainers("A");
  // The order of retainers is unspecified, so we check string length, and
  // verify each retainer separately.
  CHECK_EQ(static_cast<int>(strlen("(global property);1,B;2,C;2")),
           static_cast<int>(strlen(retainers_of_a)));
  CHECK(strstr(retainers_of_a, "(global property);1") != NULL);
  CHECK(strstr(retainers_of_a, "B;2") != NULL);
  CHECK(strstr(retainers_of_a, "C;2") != NULL);
  CHECK_EQ("(global property);2", printer.GetRetainers("B"));
  CHECK_EQ("(global property);1", printer.GetRetainers("C"));
}

#endif  // ENABLE_LOGGING_AND_PROFILING
