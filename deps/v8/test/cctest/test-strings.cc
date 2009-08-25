// Copyright 2006-2008 the V8 project authors. All rights reserved.

// Check that we can traverse very deep stacks of ConsStrings using
// StringInputBuffer.  Check that Get(int) works on very deep stacks
// of ConsStrings.  These operations may not be very fast, but they
// should be possible without getting errors due to too deep recursion.

#include <stdlib.h>

#include "v8.h"

#include "api.h"
#include "factory.h"
#include "cctest.h"
#include "zone-inl.h"

unsigned int seed = 123;

static uint32_t gen() {
        uint64_t z;
        z = seed;
        z *= 279470273;
        z %= 4294967291U;
        seed = static_cast<unsigned int>(z);
        return static_cast<uint32_t>(seed >> 16);
}


using namespace v8::internal;

static v8::Persistent<v8::Context> env;


static void InitializeVM() {
  if (env.IsEmpty()) {
    v8::HandleScope scope;
    const char* extensions[] = { "v8/print" };
    v8::ExtensionConfiguration config(1, extensions);
    env = v8::Context::New(&config);
  }
  v8::HandleScope scope;
  env->Enter();
}


static const int NUMBER_OF_BUILDING_BLOCKS = 128;
static const int DEEP_DEPTH = 8 * 1024;
static const int SUPER_DEEP_DEPTH = 80 * 1024;


static void InitializeBuildingBlocks(
    Handle<String> building_blocks[NUMBER_OF_BUILDING_BLOCKS]) {
  // A list of pointers that we don't have any interest in cleaning up.
  // If they are reachable from a root then leak detection won't complain.
  for (int i = 0; i < NUMBER_OF_BUILDING_BLOCKS; i++) {
    int len = gen() % 16;
    if (len > 14) {
      len += 1234;
    }
    switch (gen() % 4) {
      case 0: {
        uc16 buf[2000];
        for (int j = 0; j < len; j++) {
          buf[j] = gen() % 65536;
        }
        building_blocks[i] =
            Factory::NewStringFromTwoByte(Vector<const uc16>(buf, len));
        for (int j = 0; j < len; j++) {
          CHECK_EQ(buf[j], building_blocks[i]->Get(j));
        }
        break;
      }
      case 1: {
        char buf[2000];
        for (int j = 0; j < len; j++) {
          buf[j] = gen() % 128;
        }
        building_blocks[i] =
            Factory::NewStringFromAscii(Vector<const char>(buf, len));
        for (int j = 0; j < len; j++) {
          CHECK_EQ(buf[j], building_blocks[i]->Get(j));
        }
        break;
      }
      case 2: {
        class Resource: public v8::String::ExternalStringResource,
                        public ZoneObject {
         public:
          explicit Resource(Vector<const uc16> string): data_(string.start()) {
            length_ = string.length();
          }
          virtual const uint16_t* data() const { return data_; }
          virtual size_t length() const { return length_; }

         private:
          const uc16* data_;
          size_t length_;
        };
        uc16* buf = Zone::NewArray<uc16>(len);
        for (int j = 0; j < len; j++) {
          buf[j] = gen() % 65536;
        }
        Resource* resource = new Resource(Vector<const uc16>(buf, len));
        building_blocks[i] = Factory::NewExternalStringFromTwoByte(resource);
        for (int j = 0; j < len; j++) {
          CHECK_EQ(buf[j], building_blocks[i]->Get(j));
        }
        break;
      }
      case 3: {
        char* buf = NewArray<char>(len);
        for (int j = 0; j < len; j++) {
          buf[j] = gen() % 128;
        }
        building_blocks[i] =
            Factory::NewStringFromAscii(Vector<const char>(buf, len));
        for (int j = 0; j < len; j++) {
          CHECK_EQ(buf[j], building_blocks[i]->Get(j));
        }
        DeleteArray<char>(buf);
        break;
      }
    }
  }
}


static Handle<String> ConstructLeft(
    Handle<String> building_blocks[NUMBER_OF_BUILDING_BLOCKS],
    int depth) {
  Handle<String> answer = Factory::NewStringFromAscii(CStrVector(""));
  for (int i = 0; i < depth; i++) {
    answer = Factory::NewConsString(
        answer,
        building_blocks[i % NUMBER_OF_BUILDING_BLOCKS]);
  }
  return answer;
}


static Handle<String> ConstructRight(
    Handle<String> building_blocks[NUMBER_OF_BUILDING_BLOCKS],
    int depth) {
  Handle<String> answer = Factory::NewStringFromAscii(CStrVector(""));
  for (int i = depth - 1; i >= 0; i--) {
    answer = Factory::NewConsString(
        building_blocks[i % NUMBER_OF_BUILDING_BLOCKS],
        answer);
  }
  return answer;
}


static Handle<String> ConstructBalancedHelper(
    Handle<String> building_blocks[NUMBER_OF_BUILDING_BLOCKS],
    int from,
    int to) {
  CHECK(to > from);
  if (to - from == 1) {
    return building_blocks[from % NUMBER_OF_BUILDING_BLOCKS];
  }
  if (to - from == 2) {
    return Factory::NewConsString(
        building_blocks[from % NUMBER_OF_BUILDING_BLOCKS],
        building_blocks[(from+1) % NUMBER_OF_BUILDING_BLOCKS]);
  }
  Handle<String> part1 =
    ConstructBalancedHelper(building_blocks, from, from + ((to - from) / 2));
  Handle<String> part2 =
    ConstructBalancedHelper(building_blocks, from + ((to - from) / 2), to);
  return Factory::NewConsString(part1, part2);
}


static Handle<String> ConstructBalanced(
    Handle<String> building_blocks[NUMBER_OF_BUILDING_BLOCKS]) {
  return ConstructBalancedHelper(building_blocks, 0, DEEP_DEPTH);
}


static StringInputBuffer buffer;


static void Traverse(Handle<String> s1, Handle<String> s2) {
  int i = 0;
  buffer.Reset(*s1);
  StringInputBuffer buffer2(*s2);
  while (buffer.has_more()) {
    CHECK(buffer2.has_more());
    uint16_t c = buffer.GetNext();
    CHECK_EQ(c, buffer2.GetNext());
    i++;
  }
  CHECK_EQ(s1->length(), i);
  CHECK_EQ(s2->length(), i);
}


static void TraverseFirst(Handle<String> s1, Handle<String> s2, int chars) {
  int i = 0;
  buffer.Reset(*s1);
  StringInputBuffer buffer2(*s2);
  while (buffer.has_more() && i < chars) {
    CHECK(buffer2.has_more());
    uint16_t c = buffer.GetNext();
    CHECK_EQ(c, buffer2.GetNext());
    i++;
  }
  s1->Get(s1->length() - 1);
  s2->Get(s2->length() - 1);
}


TEST(Traverse) {
  printf("TestTraverse\n");
  InitializeVM();
  v8::HandleScope scope;
  Handle<String> building_blocks[NUMBER_OF_BUILDING_BLOCKS];
  ZoneScope zone(DELETE_ON_EXIT);
  InitializeBuildingBlocks(building_blocks);
  Handle<String> flat = ConstructBalanced(building_blocks);
  FlattenString(flat);
  Handle<String> left_asymmetric = ConstructLeft(building_blocks, DEEP_DEPTH);
  Handle<String> right_asymmetric = ConstructRight(building_blocks, DEEP_DEPTH);
  Handle<String> symmetric = ConstructBalanced(building_blocks);
  printf("1\n");
  Traverse(flat, symmetric);
  printf("2\n");
  Traverse(flat, left_asymmetric);
  printf("3\n");
  Traverse(flat, right_asymmetric);
  printf("4\n");
  Handle<String> left_deep_asymmetric =
      ConstructLeft(building_blocks, SUPER_DEEP_DEPTH);
  Handle<String> right_deep_asymmetric =
      ConstructRight(building_blocks, SUPER_DEEP_DEPTH);
  printf("5\n");
  TraverseFirst(left_asymmetric, left_deep_asymmetric, 1050);
  printf("6\n");
  TraverseFirst(left_asymmetric, right_deep_asymmetric, 65536);
  printf("7\n");
  Handle<String> right_deep_slice =
      Factory::NewStringSlice(left_deep_asymmetric,
                              left_deep_asymmetric->length() - 1050,
                              left_deep_asymmetric->length() - 50);
  Handle<String> left_deep_slice =
      Factory::NewStringSlice(right_deep_asymmetric,
                              right_deep_asymmetric->length() - 1050,
                              right_deep_asymmetric->length() - 50);
  printf("8\n");
  Traverse(right_deep_slice, left_deep_slice);
  printf("9\n");
  FlattenString(left_asymmetric);
  printf("10\n");
  Traverse(flat, left_asymmetric);
  printf("11\n");
  FlattenString(right_asymmetric);
  printf("12\n");
  Traverse(flat, right_asymmetric);
  printf("14\n");
  FlattenString(symmetric);
  printf("15\n");
  Traverse(flat, symmetric);
  printf("16\n");
  FlattenString(left_deep_asymmetric);
  printf("18\n");
}


static Handle<String> SliceOf(Handle<String> underlying) {
  int start = gen() % underlying->length();
  int end = start + gen() % (underlying->length() - start);
  return Factory::NewStringSlice(underlying,
                                 start,
                                 end);
}


static Handle<String> ConstructSliceTree(
    Handle<String> building_blocks[NUMBER_OF_BUILDING_BLOCKS],
    int from,
    int to) {
  CHECK(to > from);
  if (to - from <= 1)
    return SliceOf(building_blocks[from % NUMBER_OF_BUILDING_BLOCKS]);
  if (to - from == 2) {
    Handle<String> lhs = building_blocks[from % NUMBER_OF_BUILDING_BLOCKS];
    if (gen() % 2 == 0)
      lhs = SliceOf(lhs);
    Handle<String> rhs = building_blocks[(from+1) % NUMBER_OF_BUILDING_BLOCKS];
    if (gen() % 2 == 0)
      rhs = SliceOf(rhs);
    return Factory::NewConsString(lhs, rhs);
  }
  Handle<String> part1 =
    ConstructBalancedHelper(building_blocks, from, from + ((to - from) / 2));
  Handle<String> part2 =
    ConstructBalancedHelper(building_blocks, from + ((to - from) / 2), to);
  Handle<String> branch = Factory::NewConsString(part1, part2);
  if (gen() % 2 == 0)
    return branch;
  return(SliceOf(branch));
}


TEST(Slice) {
  printf("TestSlice\n");
  InitializeVM();
  v8::HandleScope scope;
  Handle<String> building_blocks[NUMBER_OF_BUILDING_BLOCKS];
  ZoneScope zone(DELETE_ON_EXIT);
  InitializeBuildingBlocks(building_blocks);

  seed = 42;
  Handle<String> slice_tree =
      ConstructSliceTree(building_blocks, 0, DEEP_DEPTH);
  seed = 42;
  Handle<String> flat_slice_tree =
      ConstructSliceTree(building_blocks, 0, DEEP_DEPTH);
  FlattenString(flat_slice_tree);
  Traverse(flat_slice_tree, slice_tree);
}

static const int DEEP_ASCII_DEPTH = 100000;


TEST(DeepAscii) {
  printf("TestDeepAscii\n");
  InitializeVM();
  v8::HandleScope scope;

  char* foo = NewArray<char>(DEEP_ASCII_DEPTH);
  for (int i = 0; i < DEEP_ASCII_DEPTH; i++) {
    foo[i] = "foo "[i % 4];
  }
  Handle<String> string =
      Factory::NewStringFromAscii(Vector<const char>(foo, DEEP_ASCII_DEPTH));
  Handle<String> foo_string = Factory::NewStringFromAscii(CStrVector("foo"));
  for (int i = 0; i < DEEP_ASCII_DEPTH; i += 10) {
    string = Factory::NewConsString(string, foo_string);
  }
  Handle<String> flat_string = Factory::NewConsString(string, foo_string);
  FlattenString(flat_string);

  for (int i = 0; i < 500; i++) {
    TraverseFirst(flat_string, string, DEEP_ASCII_DEPTH);
  }
  DeleteArray<char>(foo);
}


TEST(Utf8Conversion) {
  // Smoke test for converting strings to utf-8.
  InitializeVM();
  v8::HandleScope handle_scope;
  // A simple ascii string
  const char* ascii_string = "abcdef12345";
  int len = v8::String::New(ascii_string, strlen(ascii_string))->Utf8Length();
  CHECK_EQ(strlen(ascii_string), len);
  // A mixed ascii and non-ascii string
  // U+02E4 -> CB A4
  // U+0064 -> 64
  // U+12E4 -> E1 8B A4
  // U+0030 -> 30
  // U+3045 -> E3 81 85
  const uint16_t mixed_string[] = {0x02E4, 0x0064, 0x12E4, 0x0030, 0x3045};
  // The characters we expect to be output
  const unsigned char as_utf8[11] = {0xCB, 0xA4, 0x64, 0xE1, 0x8B, 0xA4, 0x30,
      0xE3, 0x81, 0x85, 0x00};
  // The number of bytes expected to be written for each length
  const int lengths[12] = {0, 0, 2, 3, 3, 3, 6, 7, 7, 7, 10, 11};
  v8::Handle<v8::String> mixed = v8::String::New(mixed_string, 5);
  CHECK_EQ(10, mixed->Utf8Length());
  // Try encoding the string with all capacities
  char buffer[11];
  const char kNoChar = static_cast<char>(-1);
  for (int i = 0; i <= 11; i++) {
    // Clear the buffer before reusing it
    for (int j = 0; j < 11; j++)
      buffer[j] = kNoChar;
    int written = mixed->WriteUtf8(buffer, i);
    CHECK_EQ(lengths[i], written);
    // Check that the contents are correct
    for (int j = 0; j < lengths[i]; j++)
      CHECK_EQ(as_utf8[j], static_cast<unsigned char>(buffer[j]));
    // Check that the rest of the buffer hasn't been touched
    for (int j = lengths[i]; j < 11; j++)
      CHECK_EQ(kNoChar, buffer[j]);
  }
}


class TwoByteResource: public v8::String::ExternalStringResource {
 public:
  TwoByteResource(const uint16_t* data, size_t length, bool* destructed)
      : data_(data), length_(length), destructed_(destructed) {
    CHECK_NE(destructed, NULL);
    *destructed_ = false;
  }

  virtual ~TwoByteResource() {
    CHECK_NE(destructed_, NULL);
    CHECK(!*destructed_);
    *destructed_ = true;
  }

  const uint16_t* data() const { return data_; }
  size_t length() const { return length_; }

 private:
  const uint16_t* data_;
  size_t length_;
  bool* destructed_;
};


// Regression test case for http://crbug.com/9746. The problem was
// that when we marked objects reachable only through weak pointers,
// we ended up keeping a sliced symbol alive, even though we already
// invoked the weak callback on the underlying external string thus
// deleting its resource.
TEST(Regress9746) {
  InitializeVM();

  // Setup lengths that guarantee we'll get slices instead of simple
  // flat strings.
  static const int kFullStringLength = String::kMinNonFlatLength * 2;
  static const int kSliceStringLength = String::kMinNonFlatLength + 1;

  uint16_t* source = new uint16_t[kFullStringLength];
  for (int i = 0; i < kFullStringLength; i++) source[i] = '1';
  char* key = new char[kSliceStringLength];
  for (int i = 0; i < kSliceStringLength; i++) key[i] = '1';
  Vector<const char> key_vector(key, kSliceStringLength);

  // Allocate an external string resource that keeps track of when it
  // is destructed.
  bool resource_destructed = false;
  TwoByteResource* resource =
      new TwoByteResource(source, kFullStringLength, &resource_destructed);

  {
    v8::HandleScope scope;

    // Allocate an external string resource and external string. We
    // have to go through the API to get the weak handle and the
    // automatic destruction going.
    Handle<String> string =
        v8::Utils::OpenHandle(*v8::String::NewExternal(resource));

    // Create a slice of the external string.
    Handle<String> slice =
        Factory::NewStringSlice(string, 0, kSliceStringLength);
    CHECK_EQ(kSliceStringLength, slice->length());
    CHECK(StringShape(*slice).IsSliced());

    // Make sure the slice ends up in old space so we can morph it
    // into a symbol.
    while (Heap::InNewSpace(*slice)) {
      Heap::PerformScavenge();
    }

    // Force the slice into the symbol table.
    slice = Factory::SymbolFromString(slice);
    CHECK(slice->IsSymbol());
    CHECK(StringShape(*slice).IsSliced());

    Handle<String> buffer(Handle<SlicedString>::cast(slice)->buffer());
    CHECK(StringShape(*buffer).IsExternal());
    CHECK(buffer->IsTwoByteRepresentation());

    // Finally, base a script on the slice of the external string and
    // get its wrapper. This allocates yet another weak handle that
    // indirectly refers to the external string.
    Handle<Script> script = Factory::NewScript(slice);
    Handle<JSObject> wrapper = GetScriptWrapper(script);
  }

  // When we collect all garbage, we cannot get rid of the sliced
  // symbol entry in the symbol table because it is used by the script
  // kept alive by the weak wrapper. Make sure we don't destruct the
  // external string.
  Heap::CollectAllGarbage(false);
  CHECK(!resource_destructed);

  {
    v8::HandleScope scope;

    // Make sure the sliced symbol is still in the table.
    Handle<String> symbol = Factory::LookupSymbol(key_vector);
    CHECK(StringShape(*symbol).IsSliced());

    // Make sure the buffer is still a two-byte external string.
    Handle<String> buffer(Handle<SlicedString>::cast(symbol)->buffer());
    CHECK(StringShape(*buffer).IsExternal());
    CHECK(buffer->IsTwoByteRepresentation());
  }

  // Forcing another garbage collection should let us get rid of the
  // slice from the symbol table. The external string remains in the
  // heap until the next GC.
  Heap::CollectAllGarbage(false);
  CHECK(!resource_destructed);
  v8::HandleScope scope;
  Handle<String> key_string = Factory::NewStringFromAscii(key_vector);
  String* out;
  CHECK(!Heap::LookupSymbolIfExists(*key_string, &out));

  // Forcing yet another garbage collection must allow us to finally
  // get rid of the external string.
  Heap::CollectAllGarbage(false);
  CHECK(resource_destructed);

  delete[] source;
  delete[] key;
}
