// Copyright 2012 the V8 project authors. All rights reserved.
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

// Check that we can traverse very deep stacks of ConsStrings using
// StringCharacterStram.  Check that Get(int) works on very deep stacks
// of ConsStrings.  These operations may not be very fast, but they
// should be possible without getting errors due to too deep recursion.

#include <stdlib.h>

#include "v8.h"

#include "api.h"
#include "factory.h"
#include "objects.h"
#include "cctest.h"
#include "zone-inl.h"

// Adapted from http://en.wikipedia.org/wiki/Multiply-with-carry
class MyRandomNumberGenerator {
 public:
  MyRandomNumberGenerator() {
    init();
  }

  void init(uint32_t seed = 0x5688c73e) {
    static const uint32_t phi = 0x9e3779b9;
    c = 362436;
    i = kQSize-1;
    Q[0] = seed;
    Q[1] = seed + phi;
    Q[2] = seed + phi + phi;
    for (unsigned j = 3; j < kQSize; j++) {
      Q[j] = Q[j - 3] ^ Q[j - 2] ^ phi ^ j;
    }
  }

  uint32_t next() {
    uint64_t a = 18782;
    uint32_t r = 0xfffffffe;
    i = (i + 1) & (kQSize-1);
    uint64_t t = a * Q[i] + c;
    c = (t >> 32);
    uint32_t x = static_cast<uint32_t>(t + c);
    if (x < c) {
      x++;
      c++;
    }
    return (Q[i] = r - x);
  }

  uint32_t next(int max) {
    return next() % max;
  }

  bool next(double threshold) {
    ASSERT(threshold >= 0.0 && threshold <= 1.0);
    if (threshold == 1.0) return true;
    if (threshold == 0.0) return false;
    uint32_t value = next() % 100000;
    return threshold > static_cast<double>(value)/100000.0;
  }

 private:
  static const uint32_t kQSize = 4096;
  uint32_t Q[kQSize];
  uint32_t c;
  uint32_t i;
};


using namespace v8::internal;


static const int DEEP_DEPTH = 8 * 1024;
static const int SUPER_DEEP_DEPTH = 80 * 1024;


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


class AsciiResource: public v8::String::ExternalAsciiStringResource,
                public ZoneObject {
 public:
  explicit AsciiResource(Vector<const char> string): data_(string.start()) {
    length_ = string.length();
  }
  virtual const char* data() const { return data_; }
  virtual size_t length() const { return length_; }

 private:
  const char* data_;
  size_t length_;
};


static void InitializeBuildingBlocks(Handle<String>* building_blocks,
                                     int bb_length,
                                     bool long_blocks,
                                     MyRandomNumberGenerator* rng,
                                     Zone* zone) {
  // A list of pointers that we don't have any interest in cleaning up.
  // If they are reachable from a root then leak detection won't complain.
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  for (int i = 0; i < bb_length; i++) {
    int len = rng->next(16);
    int slice_head_chars = 0;
    int slice_tail_chars = 0;
    int slice_depth = 0;
    for (int j = 0; j < 3; j++) {
      if (rng->next(0.35)) slice_depth++;
    }
    // Must truncate something for a slice string. Loop until
    // at least one end will be sliced.
    while (slice_head_chars == 0 && slice_tail_chars == 0) {
      slice_head_chars = rng->next(15);
      slice_tail_chars = rng->next(12);
    }
    if (long_blocks) {
      // Generate building blocks which will never be merged
      len += ConsString::kMinLength + 1;
    } else if (len > 14) {
      len += 1234;
    }
    // Don't slice 0 length strings.
    if (len == 0) slice_depth = 0;
    int slice_length = slice_depth*(slice_head_chars + slice_tail_chars);
    len += slice_length;
    switch (rng->next(4)) {
      case 0: {
        uc16 buf[2000];
        for (int j = 0; j < len; j++) {
          buf[j] = rng->next(0x10000);
        }
        building_blocks[i] =
            factory->NewStringFromTwoByte(Vector<const uc16>(buf, len));
        for (int j = 0; j < len; j++) {
          CHECK_EQ(buf[j], building_blocks[i]->Get(j));
        }
        break;
      }
      case 1: {
        char buf[2000];
        for (int j = 0; j < len; j++) {
          buf[j] = rng->next(0x80);
        }
        building_blocks[i] =
            factory->NewStringFromAscii(Vector<const char>(buf, len));
        for (int j = 0; j < len; j++) {
          CHECK_EQ(buf[j], building_blocks[i]->Get(j));
        }
        break;
      }
      case 2: {
        uc16* buf = zone->NewArray<uc16>(len);
        for (int j = 0; j < len; j++) {
          buf[j] = rng->next(0x10000);
        }
        Resource* resource = new(zone) Resource(Vector<const uc16>(buf, len));
        building_blocks[i] = factory->NewExternalStringFromTwoByte(resource);
        for (int j = 0; j < len; j++) {
          CHECK_EQ(buf[j], building_blocks[i]->Get(j));
        }
        break;
      }
      case 3: {
        char* buf = zone->NewArray<char>(len);
        for (int j = 0; j < len; j++) {
          buf[j] = rng->next(0x80);
        }
        AsciiResource* resource =
            new(zone) AsciiResource(Vector<const char>(buf, len));
        building_blocks[i] = factory->NewExternalStringFromAscii(resource);
        for (int j = 0; j < len; j++) {
          CHECK_EQ(buf[j], building_blocks[i]->Get(j));
        }
        break;
      }
    }
    for (int j = slice_depth; j > 0; j--) {
      building_blocks[i] = factory->NewSubString(
          building_blocks[i],
          slice_head_chars,
          building_blocks[i]->length() - slice_tail_chars);
    }
    CHECK(len == building_blocks[i]->length() + slice_length);
  }
}


class ConsStringStats {
 public:
  ConsStringStats() {
    Reset();
  }
  void Reset();
  void VerifyEqual(const ConsStringStats& that) const;
  unsigned leaves_;
  unsigned empty_leaves_;
  unsigned chars_;
  unsigned left_traversals_;
  unsigned right_traversals_;
 private:
  DISALLOW_COPY_AND_ASSIGN(ConsStringStats);
};


void ConsStringStats::Reset() {
  leaves_ = 0;
  empty_leaves_ = 0;
  chars_ = 0;
  left_traversals_ = 0;
  right_traversals_ = 0;
}


void ConsStringStats::VerifyEqual(const ConsStringStats& that) const {
  CHECK(this->leaves_ == that.leaves_);
  CHECK(this->empty_leaves_ == that.empty_leaves_);
  CHECK(this->chars_ == that.chars_);
  CHECK(this->left_traversals_ == that.left_traversals_);
  CHECK(this->right_traversals_ == that.right_traversals_);
}


class ConsStringGenerationData {
 public:
  static const int kNumberOfBuildingBlocks = 256;
  ConsStringGenerationData(bool long_blocks, Zone* zone);
  void Reset();
  inline Handle<String> block(int offset);
  inline Handle<String> block(uint32_t offset);
  // Input variables.
  double early_termination_threshold_;
  double leftness_;
  double rightness_;
  double empty_leaf_threshold_;
  unsigned max_leaves_;
  // Cached data.
  Handle<String> building_blocks_[kNumberOfBuildingBlocks];
  String* empty_string_;
  MyRandomNumberGenerator rng_;
  // Stats.
  ConsStringStats stats_;
  unsigned early_terminations_;
 private:
  DISALLOW_COPY_AND_ASSIGN(ConsStringGenerationData);
};


ConsStringGenerationData::ConsStringGenerationData(bool long_blocks,
                                                   Zone* zone) {
  rng_.init();
  InitializeBuildingBlocks(
      building_blocks_, kNumberOfBuildingBlocks, long_blocks, &rng_, zone);
  empty_string_ = CcTest::heap()->empty_string();
  Reset();
}


Handle<String> ConsStringGenerationData::block(uint32_t offset) {
  return building_blocks_[offset % kNumberOfBuildingBlocks ];
}


Handle<String> ConsStringGenerationData::block(int offset) {
  CHECK_GE(offset, 0);
  return building_blocks_[offset % kNumberOfBuildingBlocks];
}


void ConsStringGenerationData::Reset() {
  early_termination_threshold_ = 0.01;
  leftness_ = 0.75;
  rightness_ = 0.75;
  empty_leaf_threshold_ = 0.02;
  max_leaves_ = 1000;
  stats_.Reset();
  early_terminations_ = 0;
  rng_.init();
}


void AccumulateStats(ConsString* cons_string, ConsStringStats* stats) {
  int left_length = cons_string->first()->length();
  int right_length = cons_string->second()->length();
  CHECK(cons_string->length() == left_length + right_length);
  // Check left side.
  bool left_is_cons = cons_string->first()->IsConsString();
  if (left_is_cons) {
    stats->left_traversals_++;
    AccumulateStats(ConsString::cast(cons_string->first()), stats);
  } else {
    CHECK_NE(left_length, 0);
    stats->leaves_++;
    stats->chars_ += left_length;
  }
  // Check right side.
  if (cons_string->second()->IsConsString()) {
    stats->right_traversals_++;
    AccumulateStats(ConsString::cast(cons_string->second()), stats);
  } else {
    if (right_length == 0) {
      stats->empty_leaves_++;
      CHECK(!left_is_cons);
    }
    stats->leaves_++;
    stats->chars_ += right_length;
  }
}


void AccumulateStats(Handle<String> cons_string, ConsStringStats* stats) {
  DisallowHeapAllocation no_allocation;
  if (cons_string->IsConsString()) {
    return AccumulateStats(ConsString::cast(*cons_string), stats);
  }
  // This string got flattened by gc.
  stats->chars_ += cons_string->length();
}


void AccumulateStatsWithOperator(
    ConsString* cons_string, ConsStringStats* stats) {
  unsigned offset = 0;
  int32_t type = cons_string->map()->instance_type();
  unsigned length = static_cast<unsigned>(cons_string->length());
  ConsStringIteratorOp op;
  String* string = op.Operate(cons_string, &offset, &type, &length);
  CHECK(string != NULL);
  while (true) {
    ASSERT(!string->IsConsString());
    // Accumulate stats.
    stats->leaves_++;
    stats->chars_ += string->length();
    // Check for completion.
    bool keep_going_fast_check = op.HasMore();
    string = op.ContinueOperation(&type, &length);
    if (string == NULL) return;
    // Verify no false positives for fast check.
    CHECK(keep_going_fast_check);
  }
}


void VerifyConsString(Handle<String> root, ConsStringGenerationData* data) {
  // Verify basic data.
  CHECK(root->IsConsString());
  CHECK(static_cast<unsigned>(root->length()) == data->stats_.chars_);
  // Recursive verify.
  ConsStringStats stats;
  AccumulateStats(ConsString::cast(*root), &stats);
  stats.VerifyEqual(data->stats_);
  // Iteratively verify.
  stats.Reset();
  AccumulateStatsWithOperator(ConsString::cast(*root), &stats);
  // Don't see these. Must copy over.
  stats.empty_leaves_ = data->stats_.empty_leaves_;
  stats.left_traversals_ = data->stats_.left_traversals_;
  stats.right_traversals_ = data->stats_.right_traversals_;
  // Adjust total leaves to compensate.
  stats.leaves_ += stats.empty_leaves_;
  stats.VerifyEqual(data->stats_);
}


static Handle<String> ConstructRandomString(ConsStringGenerationData* data,
                                            unsigned max_recursion) {
  Factory* factory = CcTest::i_isolate()->factory();
  // Compute termination characteristics.
  bool terminate = false;
  bool flat = data->rng_.next(data->empty_leaf_threshold_);
  bool terminate_early = data->rng_.next(data->early_termination_threshold_);
  if (terminate_early) data->early_terminations_++;
  // The obvious condition.
  terminate |= max_recursion == 0;
  // Flat cons string terminate by definition.
  terminate |= flat;
  // Cap for max leaves.
  terminate |= data->stats_.leaves_ >= data->max_leaves_;
  // Roll the dice.
  terminate |= terminate_early;
  // Compute termination characteristics for each side.
  bool terminate_left = terminate || !data->rng_.next(data->leftness_);
  bool terminate_right = terminate || !data->rng_.next(data->rightness_);
  // Generate left string.
  Handle<String> left;
  if (terminate_left) {
    left = data->block(data->rng_.next());
    data->stats_.leaves_++;
    data->stats_.chars_ += left->length();
  } else {
    data->stats_.left_traversals_++;
  }
  // Generate right string.
  Handle<String> right;
  if (terminate_right) {
    right = data->block(data->rng_.next());
    data->stats_.leaves_++;
    data->stats_.chars_ += right->length();
  } else {
    data->stats_.right_traversals_++;
  }
  // Generate the necessary sub-nodes recursively.
  if (!terminate_right) {
    // Need to balance generation fairly.
    if (!terminate_left && data->rng_.next(0.5)) {
      left = ConstructRandomString(data, max_recursion - 1);
    }
    right = ConstructRandomString(data, max_recursion - 1);
  }
  if (!terminate_left && left.is_null()) {
    left = ConstructRandomString(data, max_recursion - 1);
  }
  // Build the cons string.
  Handle<String> root = factory->NewConsString(left, right);
  CHECK(root->IsConsString() && !root->IsFlat());
  // Special work needed for flat string.
  if (flat) {
    data->stats_.empty_leaves_++;
    FlattenString(root);
    CHECK(root->IsConsString() && root->IsFlat());
  }
  return root;
}


static Handle<String> ConstructLeft(
    ConsStringGenerationData* data,
    int depth) {
  Factory* factory = CcTest::i_isolate()->factory();
  Handle<String> answer = factory->NewStringFromAscii(CStrVector(""));
  data->stats_.leaves_++;
  for (int i = 0; i < depth; i++) {
    Handle<String> block = data->block(i);
    Handle<String> next = factory->NewConsString(answer, block);
    if (next->IsConsString()) data->stats_.leaves_++;
    data->stats_.chars_ += block->length();
    answer = next;
  }
  data->stats_.left_traversals_ = data->stats_.leaves_ - 2;
  return answer;
}


static Handle<String> ConstructRight(
    ConsStringGenerationData* data,
    int depth) {
  Factory* factory = CcTest::i_isolate()->factory();
  Handle<String> answer = factory->NewStringFromAscii(CStrVector(""));
  data->stats_.leaves_++;
  for (int i = depth - 1; i >= 0; i--) {
    Handle<String> block = data->block(i);
    Handle<String> next = factory->NewConsString(block, answer);
    if (next->IsConsString()) data->stats_.leaves_++;
    data->stats_.chars_ += block->length();
    answer = next;
  }
  data->stats_.right_traversals_ = data->stats_.leaves_ - 2;
  return answer;
}


static Handle<String> ConstructBalancedHelper(
    ConsStringGenerationData* data,
    int from,
    int to) {
  Factory* factory = CcTest::i_isolate()->factory();
  CHECK(to > from);
  if (to - from == 1) {
    data->stats_.chars_ += data->block(from)->length();
    return data->block(from);
  }
  if (to - from == 2) {
    data->stats_.chars_ += data->block(from)->length();
    data->stats_.chars_ += data->block(from+1)->length();
    return factory->NewConsString(data->block(from), data->block(from+1));
  }
  Handle<String> part1 =
    ConstructBalancedHelper(data, from, from + ((to - from) / 2));
  Handle<String> part2 =
    ConstructBalancedHelper(data, from + ((to - from) / 2), to);
  if (part1->IsConsString()) data->stats_.left_traversals_++;
  if (part2->IsConsString()) data->stats_.right_traversals_++;
  return factory->NewConsString(part1, part2);
}


static Handle<String> ConstructBalanced(
    ConsStringGenerationData* data, int depth = DEEP_DEPTH) {
  Handle<String> string = ConstructBalancedHelper(data, 0, depth);
  data->stats_.leaves_ =
      data->stats_.left_traversals_ + data->stats_.right_traversals_ + 2;
  return string;
}


static ConsStringIteratorOp cons_string_iterator_op_1;
static ConsStringIteratorOp cons_string_iterator_op_2;

static void Traverse(Handle<String> s1, Handle<String> s2) {
  int i = 0;
  StringCharacterStream character_stream_1(*s1, &cons_string_iterator_op_1);
  StringCharacterStream character_stream_2(*s2, &cons_string_iterator_op_2);
  while (character_stream_1.HasMore()) {
    CHECK(character_stream_2.HasMore());
    uint16_t c = character_stream_1.GetNext();
    CHECK_EQ(c, character_stream_2.GetNext());
    i++;
  }
  CHECK(!character_stream_1.HasMore());
  CHECK(!character_stream_2.HasMore());
  CHECK_EQ(s1->length(), i);
  CHECK_EQ(s2->length(), i);
}


static void TraverseFirst(Handle<String> s1, Handle<String> s2, int chars) {
  int i = 0;
  StringCharacterStream character_stream_1(*s1, &cons_string_iterator_op_1);
  StringCharacterStream character_stream_2(*s2, &cons_string_iterator_op_2);
  while (character_stream_1.HasMore() && i < chars) {
    CHECK(character_stream_2.HasMore());
    uint16_t c = character_stream_1.GetNext();
    CHECK_EQ(c, character_stream_2.GetNext());
    i++;
  }
  s1->Get(s1->length() - 1);
  s2->Get(s2->length() - 1);
}


TEST(Traverse) {
  printf("TestTraverse\n");
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Zone zone(CcTest::i_isolate());
  ConsStringGenerationData data(false, &zone);
  Handle<String> flat = ConstructBalanced(&data);
  FlattenString(flat);
  Handle<String> left_asymmetric = ConstructLeft(&data, DEEP_DEPTH);
  Handle<String> right_asymmetric = ConstructRight(&data, DEEP_DEPTH);
  Handle<String> symmetric = ConstructBalanced(&data);
  printf("1\n");
  Traverse(flat, symmetric);
  printf("2\n");
  Traverse(flat, left_asymmetric);
  printf("3\n");
  Traverse(flat, right_asymmetric);
  printf("4\n");
  Handle<String> left_deep_asymmetric =
      ConstructLeft(&data, SUPER_DEEP_DEPTH);
  Handle<String> right_deep_asymmetric =
      ConstructRight(&data, SUPER_DEEP_DEPTH);
  printf("5\n");
  TraverseFirst(left_asymmetric, left_deep_asymmetric, 1050);
  printf("6\n");
  TraverseFirst(left_asymmetric, right_deep_asymmetric, 65536);
  printf("7\n");
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


static void VerifyCharacterStream(
    String* flat_string, String* cons_string) {
  // Do not want to test ConString traversal on flat string.
  CHECK(flat_string->IsFlat() && !flat_string->IsConsString());
  CHECK(cons_string->IsConsString());
  // TODO(dcarney) Test stream reset as well.
  int length = flat_string->length();
  // Iterate start search in multiple places in the string.
  int outer_iterations = length > 20 ? 20 : length;
  for (int j = 0; j <= outer_iterations; j++) {
    int offset = length * j / outer_iterations;
    if (offset < 0) offset = 0;
    // Want to test the offset == length case.
    if (offset > length) offset = length;
    StringCharacterStream flat_stream(
        flat_string, &cons_string_iterator_op_1, static_cast<unsigned>(offset));
    StringCharacterStream cons_stream(
        cons_string, &cons_string_iterator_op_2, static_cast<unsigned>(offset));
    for (int i = offset; i < length; i++) {
      uint16_t c = flat_string->Get(i);
      CHECK(flat_stream.HasMore());
      CHECK(cons_stream.HasMore());
      CHECK_EQ(c, flat_stream.GetNext());
      CHECK_EQ(c, cons_stream.GetNext());
    }
    CHECK(!flat_stream.HasMore());
    CHECK(!cons_stream.HasMore());
  }
}


static inline void PrintStats(const ConsStringGenerationData& data) {
#ifdef DEBUG
printf(
    "%s: [%d], %s: [%d], %s: [%d], %s: [%d], %s: [%d], %s: [%d]\n",
    "leaves", data.stats_.leaves_,
    "empty", data.stats_.empty_leaves_,
    "chars", data.stats_.chars_,
    "lefts", data.stats_.left_traversals_,
    "rights", data.stats_.right_traversals_,
    "early_terminations", data.early_terminations_);
#endif
}


template<typename BuildString>
void TestStringCharacterStream(BuildString build, int test_cases) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Zone zone(isolate);
  ConsStringGenerationData data(true, &zone);
  for (int i = 0; i < test_cases; i++) {
    printf("%d\n", i);
    HandleScope inner_scope(isolate);
    AlwaysAllocateScope always_allocate;
    // Build flat version of cons string.
    Handle<String> flat_string = build(i, &data);
    ConsStringStats flat_string_stats;
    AccumulateStats(flat_string, &flat_string_stats);
    // Flatten string.
    FlattenString(flat_string);
    // Build unflattened version of cons string to test.
    Handle<String> cons_string = build(i, &data);
    ConsStringStats cons_string_stats;
    AccumulateStats(cons_string, &cons_string_stats);
    DisallowHeapAllocation no_allocation;
    PrintStats(data);
    // Full verify of cons string.
    cons_string_stats.VerifyEqual(flat_string_stats);
    cons_string_stats.VerifyEqual(data.stats_);
    VerifyConsString(cons_string, &data);
    String* flat_string_ptr =
        flat_string->IsConsString() ?
        ConsString::cast(*flat_string)->first() :
        *flat_string;
    VerifyCharacterStream(flat_string_ptr, *cons_string);
  }
}


static const int kCharacterStreamNonRandomCases = 8;


static Handle<String> BuildEdgeCaseConsString(
    int test_case, ConsStringGenerationData* data) {
  Factory* factory = CcTest::i_isolate()->factory();
  data->Reset();
  switch (test_case) {
    case 0:
      return ConstructBalanced(data, 71);
    case 1:
      return ConstructLeft(data, 71);
    case 2:
      return ConstructRight(data, 71);
    case 3:
      return ConstructLeft(data, 10);
    case 4:
      return ConstructRight(data, 10);
    case 5:
      // 2 element balanced tree.
      data->stats_.chars_ += data->block(0)->length();
      data->stats_.chars_ += data->block(1)->length();
      data->stats_.leaves_ += 2;
      return factory->NewConsString(data->block(0), data->block(1));
    case 6:
      // Simple flattened tree.
      data->stats_.chars_ += data->block(0)->length();
      data->stats_.chars_ += data->block(1)->length();
      data->stats_.leaves_ += 2;
      data->stats_.empty_leaves_ += 1;
      {
        Handle<String> string =
            factory->NewConsString(data->block(0), data->block(1));
        FlattenString(string);
        return string;
      }
    case 7:
      // Left node flattened.
      data->stats_.chars_ += data->block(0)->length();
      data->stats_.chars_ += data->block(1)->length();
      data->stats_.chars_ += data->block(2)->length();
      data->stats_.leaves_ += 3;
      data->stats_.empty_leaves_ += 1;
      data->stats_.left_traversals_ += 1;
      {
        Handle<String> left =
            factory->NewConsString(data->block(0), data->block(1));
        FlattenString(left);
        return factory->NewConsString(left, data->block(2));
      }
    case 8:
      // Left node and right node flattened.
      data->stats_.chars_ += data->block(0)->length();
      data->stats_.chars_ += data->block(1)->length();
      data->stats_.chars_ += data->block(2)->length();
      data->stats_.chars_ += data->block(3)->length();
      data->stats_.leaves_ += 4;
      data->stats_.empty_leaves_ += 2;
      data->stats_.left_traversals_ += 1;
      data->stats_.right_traversals_ += 1;
      {
        Handle<String> left =
            factory->NewConsString(data->block(0), data->block(1));
        FlattenString(left);
        Handle<String> right =
            factory->NewConsString(data->block(2), data->block(2));
        FlattenString(right);
        return factory->NewConsString(left, right);
      }
  }
  UNREACHABLE();
  return Handle<String>();
}


TEST(StringCharacterStreamEdgeCases) {
  printf("TestStringCharacterStreamEdgeCases\n");
  TestStringCharacterStream(
      BuildEdgeCaseConsString, kCharacterStreamNonRandomCases);
}


static const int kBalances = 3;
static const int kTreeLengths = 4;
static const int kEmptyLeaves = 4;
static const int kUniqueRandomParameters =
    kBalances*kTreeLengths*kEmptyLeaves;


static void InitializeGenerationData(
    int test_case, ConsStringGenerationData* data) {
  // Clear the settings and reinit the rng.
  data->Reset();
  // Spin up the rng to a known location that is unique per test.
  static const int kPerTestJump = 501;
  for (int j = 0; j < test_case*kPerTestJump; j++) {
    data->rng_.next();
  }
  // Choose balanced, left or right heavy trees.
  switch (test_case % kBalances) {
    case 0:
      // Nothing to do.  Already balanced.
      break;
    case 1:
      // Left balanced.
      data->leftness_ = 0.90;
      data->rightness_ = 0.15;
      break;
    case 2:
      // Right balanced.
      data->leftness_ = 0.15;
      data->rightness_ = 0.90;
      break;
    default:
      UNREACHABLE();
      break;
  }
  // Must remove the influence of the above decision.
  test_case /= kBalances;
  // Choose tree length.
  switch (test_case % kTreeLengths) {
    case 0:
      data->max_leaves_ = 16;
      data->early_termination_threshold_ = 0.2;
      break;
    case 1:
      data->max_leaves_ = 50;
      data->early_termination_threshold_ = 0.05;
      break;
    case 2:
      data->max_leaves_ = 500;
      data->early_termination_threshold_ = 0.03;
      break;
    case 3:
      data->max_leaves_ = 5000;
      data->early_termination_threshold_ = 0.001;
      break;
    default:
      UNREACHABLE();
      break;
  }
  // Must remove the influence of the above decision.
  test_case /= kTreeLengths;
  // Choose how much we allow empty nodes, including not at all.
  data->empty_leaf_threshold_ =
      0.03 * static_cast<double>(test_case % kEmptyLeaves);
}


static Handle<String> BuildRandomConsString(
    int test_case, ConsStringGenerationData* data) {
  InitializeGenerationData(test_case, data);
  return ConstructRandomString(data, 200);
}


TEST(StringCharacterStreamRandom) {
  printf("StringCharacterStreamRandom\n");
  TestStringCharacterStream(BuildRandomConsString, kUniqueRandomParameters*7);
}


static const int DEEP_ASCII_DEPTH = 100000;


TEST(DeepAscii) {
  printf("TestDeepAscii\n");
  CcTest::InitializeVM();
  Factory* factory = CcTest::i_isolate()->factory();
  v8::HandleScope scope(CcTest::isolate());

  char* foo = NewArray<char>(DEEP_ASCII_DEPTH);
  for (int i = 0; i < DEEP_ASCII_DEPTH; i++) {
    foo[i] = "foo "[i % 4];
  }
  Handle<String> string =
      factory->NewStringFromAscii(Vector<const char>(foo, DEEP_ASCII_DEPTH));
  Handle<String> foo_string = factory->NewStringFromAscii(CStrVector("foo"));
  for (int i = 0; i < DEEP_ASCII_DEPTH; i += 10) {
    string = factory->NewConsString(string, foo_string);
  }
  Handle<String> flat_string = factory->NewConsString(string, foo_string);
  FlattenString(flat_string);

  for (int i = 0; i < 500; i++) {
    TraverseFirst(flat_string, string, DEEP_ASCII_DEPTH);
  }
  DeleteArray<char>(foo);
}


TEST(Utf8Conversion) {
  // Smoke test for converting strings to utf-8.
  CcTest::InitializeVM();
  v8::HandleScope handle_scope(CcTest::isolate());
  // A simple ascii string
  const char* ascii_string = "abcdef12345";
  int len =
      v8::String::New(ascii_string,
                      StrLength(ascii_string))->Utf8Length();
  CHECK_EQ(StrLength(ascii_string), len);
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
  const int char_lengths[12] = {0, 0, 1, 2, 2, 2, 3, 4, 4, 4, 5, 5};
  v8::Handle<v8::String> mixed = v8::String::New(mixed_string, 5);
  CHECK_EQ(10, mixed->Utf8Length());
  // Try encoding the string with all capacities
  char buffer[11];
  const char kNoChar = static_cast<char>(-1);
  for (int i = 0; i <= 11; i++) {
    // Clear the buffer before reusing it
    for (int j = 0; j < 11; j++)
      buffer[j] = kNoChar;
    int chars_written;
    int written = mixed->WriteUtf8(buffer, i, &chars_written);
    CHECK_EQ(lengths[i], written);
    CHECK_EQ(char_lengths[i], chars_written);
    // Check that the contents are correct
    for (int j = 0; j < lengths[i]; j++)
      CHECK_EQ(as_utf8[j], static_cast<unsigned char>(buffer[j]));
    // Check that the rest of the buffer hasn't been touched
    for (int j = lengths[i]; j < 11; j++)
      CHECK_EQ(kNoChar, buffer[j]);
  }
}


TEST(ExternalShortStringAdd) {
  Isolate* isolate = CcTest::i_isolate();
  Zone zone(isolate);

  LocalContext context;
  v8::HandleScope handle_scope(CcTest::isolate());

  // Make sure we cover all always-flat lengths and at least one above.
  static const int kMaxLength = 20;
  CHECK_GT(kMaxLength, i::ConsString::kMinLength);

  // Allocate two JavaScript arrays for holding short strings.
  v8::Handle<v8::Array> ascii_external_strings =
      v8::Array::New(kMaxLength + 1);
  v8::Handle<v8::Array> non_ascii_external_strings =
      v8::Array::New(kMaxLength + 1);

  // Generate short ascii and non-ascii external strings.
  for (int i = 0; i <= kMaxLength; i++) {
    char* ascii = zone.NewArray<char>(i + 1);
    for (int j = 0; j < i; j++) {
      ascii[j] = 'a';
    }
    // Terminating '\0' is left out on purpose. It is not required for external
    // string data.
    AsciiResource* ascii_resource =
        new(&zone) AsciiResource(Vector<const char>(ascii, i));
    v8::Local<v8::String> ascii_external_string =
        v8::String::NewExternal(ascii_resource);

    ascii_external_strings->Set(v8::Integer::New(i), ascii_external_string);
    uc16* non_ascii = zone.NewArray<uc16>(i + 1);
    for (int j = 0; j < i; j++) {
      non_ascii[j] = 0x1234;
    }
    // Terminating '\0' is left out on purpose. It is not required for external
    // string data.
    Resource* resource = new(&zone) Resource(Vector<const uc16>(non_ascii, i));
    v8::Local<v8::String> non_ascii_external_string =
      v8::String::NewExternal(resource);
    non_ascii_external_strings->Set(v8::Integer::New(i),
                                    non_ascii_external_string);
  }

  // Add the arrays with the short external strings in the global object.
  v8::Handle<v8::Object> global = context->Global();
  global->Set(v8_str("external_ascii"), ascii_external_strings);
  global->Set(v8_str("external_non_ascii"), non_ascii_external_strings);
  global->Set(v8_str("max_length"), v8::Integer::New(kMaxLength));

  // Add short external ascii and non-ascii strings checking the result.
  static const char* source =
    "function test() {"
    "  var ascii_chars = 'aaaaaaaaaaaaaaaaaaaa';"
    "  var non_ascii_chars = '\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234\\u1234';"  //NOLINT
    "  if (ascii_chars.length != max_length) return 1;"
    "  if (non_ascii_chars.length != max_length) return 2;"
    "  var ascii = Array(max_length + 1);"
    "  var non_ascii = Array(max_length + 1);"
    "  for (var i = 0; i <= max_length; i++) {"
    "    ascii[i] = ascii_chars.substring(0, i);"
    "    non_ascii[i] = non_ascii_chars.substring(0, i);"
    "  };"
    "  for (var i = 0; i <= max_length; i++) {"
    "    if (ascii[i] != external_ascii[i]) return 3;"
    "    if (non_ascii[i] != external_non_ascii[i]) return 4;"
    "    for (var j = 0; j < i; j++) {"
    "      if (external_ascii[i] !="
    "          (external_ascii[j] + external_ascii[i - j])) return 5;"
    "      if (external_non_ascii[i] !="
    "          (external_non_ascii[j] + external_non_ascii[i - j])) return 6;"
    "      if (non_ascii[i] != (non_ascii[j] + non_ascii[i - j])) return 7;"
    "      if (ascii[i] != (ascii[j] + ascii[i - j])) return 8;"
    "      if (ascii[i] != (external_ascii[j] + ascii[i - j])) return 9;"
    "      if (ascii[i] != (ascii[j] + external_ascii[i - j])) return 10;"
    "      if (non_ascii[i] !="
    "          (external_non_ascii[j] + non_ascii[i - j])) return 11;"
    "      if (non_ascii[i] !="
    "          (non_ascii[j] + external_non_ascii[i - j])) return 12;"
    "    }"
    "  }"
    "  return 0;"
    "};"
    "test()";
  CHECK_EQ(0, CompileRun(source)->Int32Value());
}


TEST(JSONStringifySliceMadeExternal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Zone zone(isolate);
  // Create a sliced string from a one-byte string.  The latter is turned
  // into a two-byte external string.  Check that JSON.stringify works.
  v8::HandleScope handle_scope(CcTest::isolate());
  v8::Handle<v8::String> underlying =
      CompileRun("var underlying = 'abcdefghijklmnopqrstuvwxyz';"
                 "underlying")->ToString();
  v8::Handle<v8::String> slice =
      CompileRun("var slice = underlying.slice(1);"
                 "slice")->ToString();
  CHECK(v8::Utils::OpenHandle(*slice)->IsSlicedString());
  CHECK(v8::Utils::OpenHandle(*underlying)->IsSeqOneByteString());

  int length = underlying->Length();
  uc16* two_byte = zone.NewArray<uc16>(length + 1);
  underlying->Write(two_byte);
  Resource* resource =
      new(&zone) Resource(Vector<const uc16>(two_byte, length));
  CHECK(underlying->MakeExternal(resource));
  CHECK(v8::Utils::OpenHandle(*slice)->IsSlicedString());
  CHECK(v8::Utils::OpenHandle(*underlying)->IsExternalTwoByteString());

  CHECK_EQ("\"bcdefghijklmnopqrstuvwxyz\"",
           *v8::String::Utf8Value(CompileRun("JSON.stringify(slice)")));
}


TEST(CachedHashOverflow) {
  CcTest::InitializeVM();
  // We incorrectly allowed strings to be tagged as array indices even if their
  // values didn't fit in the hash field.
  // See http://code.google.com/p/v8/issues/detail?id=728
  Isolate* isolate = CcTest::i_isolate();
  Zone zone(isolate);

  v8::HandleScope handle_scope(CcTest::isolate());
  // Lines must be executed sequentially. Combining them into one script
  // makes the bug go away.
  const char* lines[] = {
      "var x = [];",
      "x[4] = 42;",
      "var s = \"1073741828\";",
      "x[s];",
      "x[s] = 37;",
      "x[4];",
      "x[s];",
      NULL
  };

  Handle<Smi> fortytwo(Smi::FromInt(42), isolate);
  Handle<Smi> thirtyseven(Smi::FromInt(37), isolate);
  Handle<Object> results[] = { isolate->factory()->undefined_value(),
                               fortytwo,
                               isolate->factory()->undefined_value(),
                               isolate->factory()->undefined_value(),
                               thirtyseven,
                               fortytwo,
                               thirtyseven  // Bug yielded 42 here.
  };

  const char* line;
  for (int i = 0; (line = lines[i]); i++) {
    printf("%s\n", line);
    v8::Local<v8::Value> result =
        v8::Script::Compile(v8::String::New(line))->Run();
    CHECK_EQ(results[i]->IsUndefined(), result->IsUndefined());
    CHECK_EQ(results[i]->IsNumber(), result->IsNumber());
    if (result->IsNumber()) {
      CHECK_EQ(Smi::cast(results[i]->ToSmi()->ToObjectChecked())->value(),
               result->ToInt32()->Value());
    }
  }
}


TEST(SliceFromCons) {
  FLAG_string_slices = true;
  CcTest::InitializeVM();
  Factory* factory = CcTest::i_isolate()->factory();
  v8::HandleScope scope(CcTest::isolate());
  Handle<String> string =
      factory->NewStringFromAscii(CStrVector("parentparentparent"));
  Handle<String> parent = factory->NewConsString(string, string);
  CHECK(parent->IsConsString());
  CHECK(!parent->IsFlat());
  Handle<String> slice = factory->NewSubString(parent, 1, 25);
  // After slicing, the original string becomes a flat cons.
  CHECK(parent->IsFlat());
  CHECK(slice->IsSlicedString());
  CHECK_EQ(SlicedString::cast(*slice)->parent(),
           // Parent could have been short-circuited.
           parent->IsConsString() ? ConsString::cast(*parent)->first()
                                  : *parent);
  CHECK(SlicedString::cast(*slice)->parent()->IsSeqString());
  CHECK(slice->IsFlat());
}


class AsciiVectorResource : public v8::String::ExternalAsciiStringResource {
 public:
  explicit AsciiVectorResource(i::Vector<const char> vector)
      : data_(vector) {}
  virtual ~AsciiVectorResource() {}
  virtual size_t length() const { return data_.length(); }
  virtual const char* data() const { return data_.start(); }
 private:
  i::Vector<const char> data_;
};


TEST(SliceFromExternal) {
  FLAG_string_slices = true;
  CcTest::InitializeVM();
  Factory* factory = CcTest::i_isolate()->factory();
  v8::HandleScope scope(CcTest::isolate());
  AsciiVectorResource resource(
      i::Vector<const char>("abcdefghijklmnopqrstuvwxyz", 26));
  Handle<String> string = factory->NewExternalStringFromAscii(&resource);
  CHECK(string->IsExternalString());
  Handle<String> slice = factory->NewSubString(string, 1, 25);
  CHECK(slice->IsSlicedString());
  CHECK(string->IsExternalString());
  CHECK_EQ(SlicedString::cast(*slice)->parent(), *string);
  CHECK(SlicedString::cast(*slice)->parent()->IsExternalString());
  CHECK(slice->IsFlat());
}


TEST(TrivialSlice) {
  // This tests whether a slice that contains the entire parent string
  // actually creates a new string (it should not).
  FLAG_string_slices = true;
  CcTest::InitializeVM();
  Factory* factory = CcTest::i_isolate()->factory();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> result;
  Handle<String> string;
  const char* init = "var str = 'abcdefghijklmnopqrstuvwxyz';";
  const char* check = "str.slice(0,26)";
  const char* crosscheck = "str.slice(1,25)";

  CompileRun(init);

  result = CompileRun(check);
  CHECK(result->IsString());
  string = v8::Utils::OpenHandle(v8::String::Cast(*result));
  CHECK(!string->IsSlicedString());

  string = factory->NewSubString(string, 0, 26);
  CHECK(!string->IsSlicedString());
  result = CompileRun(crosscheck);
  CHECK(result->IsString());
  string = v8::Utils::OpenHandle(v8::String::Cast(*result));
  CHECK(string->IsSlicedString());
  CHECK_EQ("bcdefghijklmnopqrstuvwxy", *(string->ToCString()));
}


TEST(SliceFromSlice) {
  // This tests whether a slice that contains the entire parent string
  // actually creates a new string (it should not).
  FLAG_string_slices = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> result;
  Handle<String> string;
  const char* init = "var str = 'abcdefghijklmnopqrstuvwxyz';";
  const char* slice = "var slice = str.slice(1,-1); slice";
  const char* slice_from_slice = "slice.slice(1,-1);";

  CompileRun(init);
  result = CompileRun(slice);
  CHECK(result->IsString());
  string = v8::Utils::OpenHandle(v8::String::Cast(*result));
  CHECK(string->IsSlicedString());
  CHECK(SlicedString::cast(*string)->parent()->IsSeqString());
  CHECK_EQ("bcdefghijklmnopqrstuvwxy", *(string->ToCString()));

  result = CompileRun(slice_from_slice);
  CHECK(result->IsString());
  string = v8::Utils::OpenHandle(v8::String::Cast(*result));
  CHECK(string->IsSlicedString());
  CHECK(SlicedString::cast(*string)->parent()->IsSeqString());
  CHECK_EQ("cdefghijklmnopqrstuvwx", *(string->ToCString()));
}


TEST(AsciiArrayJoin) {
  // Set heap limits.
  static const int K = 1024;
  v8::ResourceConstraints constraints;
  constraints.set_max_young_space_size(256 * K);
  constraints.set_max_old_space_size(4 * K * K);
  v8::SetResourceConstraints(&constraints);

  // String s is made of 2^17 = 131072 'c' characters and a is an array
  // starting with 'bad', followed by 2^14 times the string s. That means the
  // total length of the concatenated strings is 2^31 + 3. So on 32bit systems
  // summing the lengths of the strings (as Smis) overflows and wraps.
  static const char* join_causing_out_of_memory =
      "var two_14 = Math.pow(2, 14);"
      "var two_17 = Math.pow(2, 17);"
      "var s = Array(two_17 + 1).join('c');"
      "var a = ['bad'];"
      "for (var i = 1; i <= two_14; i++) a.push(s);"
      "a.join("");";

  v8::HandleScope scope(CcTest::isolate());
  LocalContext context;
  v8::V8::IgnoreOutOfMemoryException();
  v8::Local<v8::Script> script =
      v8::Script::Compile(v8::String::New(join_causing_out_of_memory));
  v8::Local<v8::Value> result = script->Run();

  // Check for out of memory state.
  CHECK(result.IsEmpty());
  CHECK(context->HasOutOfMemoryException());
}


static void CheckException(const char* source) {
  // An empty handle is returned upon exception.
  CHECK(CompileRun(source).IsEmpty());
}


TEST(RobustSubStringStub) {
  // This tests whether the SubStringStub can handle unsafe arguments.
  // If not recognized, those unsafe arguments lead to out-of-bounds reads.
  FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> result;
  Handle<String> string;
  CompileRun("var short = 'abcdef';");

  // Invalid indices.
  CheckException("%_SubString(short,     0,    10000);");
  CheckException("%_SubString(short, -1234,        5);");
  CheckException("%_SubString(short,     5,        2);");
  // Special HeapNumbers.
  CheckException("%_SubString(short,     1, Infinity);");
  CheckException("%_SubString(short,   NaN,        5);");
  // String arguments.
  CheckException("%_SubString(short,    '2',     '5');");
  // Ordinary HeapNumbers can be handled (in runtime).
  result = CompileRun("%_SubString(short, Math.sqrt(4), 5.1);");
  string = v8::Utils::OpenHandle(v8::String::Cast(*result));
  CHECK_EQ("cde", *(string->ToCString()));

  CompileRun("var long = 'abcdefghijklmnopqrstuvwxyz';");
  // Invalid indices.
  CheckException("%_SubString(long,     0,    10000);");
  CheckException("%_SubString(long, -1234,       17);");
  CheckException("%_SubString(long,    17,        2);");
  // Special HeapNumbers.
  CheckException("%_SubString(long,     1, Infinity);");
  CheckException("%_SubString(long,   NaN,       17);");
  // String arguments.
  CheckException("%_SubString(long,    '2',    '17');");
  // Ordinary HeapNumbers within bounds can be handled (in runtime).
  result = CompileRun("%_SubString(long, Math.sqrt(4), 17.1);");
  string = v8::Utils::OpenHandle(v8::String::Cast(*result));
  CHECK_EQ("cdefghijklmnopq", *(string->ToCString()));

  // Test that out-of-bounds substring of a slice fails when the indices
  // would have been valid for the underlying string.
  CompileRun("var slice = long.slice(1, 15);");
  CheckException("%_SubString(slice, 0, 17);");
}


TEST(RegExpOverflow) {
  // Result string has the length 2^32, causing a 32-bit integer overflow.
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext context;
  v8::V8::IgnoreOutOfMemoryException();
  v8::Local<v8::Value> result = CompileRun(
      "var a = 'a';                     "
      "for (var i = 0; i < 16; i++) {   "
      "  a += a;                        "
      "}                                "
      "a.replace(/a/g, a);              ");
  CHECK(result.IsEmpty());
  CHECK(context->HasOutOfMemoryException());
}


TEST(StringReplaceAtomTwoByteResult) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext context;
  v8::Local<v8::Value> result = CompileRun(
      "var subject = 'ascii~only~string~'; "
      "var replace = '\x80';            "
      "subject.replace(/~/g, replace);  ");
  CHECK(result->IsString());
  Handle<String> string = v8::Utils::OpenHandle(v8::String::Cast(*result));
  CHECK(string->IsSeqTwoByteString());

  v8::Local<v8::String> expected = v8_str("ascii\x80only\x80string\x80");
  CHECK(expected->Equals(result));
}


TEST(IsAscii) {
  CHECK(String::IsAscii(static_cast<char*>(NULL), 0));
  CHECK(String::IsOneByte(static_cast<uc16*>(NULL), 0));
}



template<typename Op, bool return_first>
static uint16_t ConvertLatin1(uint16_t c) {
  uint32_t result[Op::kMaxWidth];
  int chars;
  chars = Op::Convert(c, 0, result, NULL);
  if (chars == 0) return 0;
  CHECK_LE(chars, static_cast<int>(sizeof(result)));
  if (!return_first && chars > 1) {
    return 0;
  }
  return result[0];
}


static void CheckCanonicalEquivalence(uint16_t c, uint16_t test) {
  uint16_t expect = ConvertLatin1<unibrow::Ecma262UnCanonicalize, true>(c);
  if (expect > unibrow::Latin1::kMaxChar) expect = 0;
  CHECK_EQ(expect, test);
}


TEST(Latin1IgnoreCase) {
  using namespace unibrow;
  for (uint16_t c = Latin1::kMaxChar + 1; c != 0; c++) {
    uint16_t lower = ConvertLatin1<ToLowercase, false>(c);
    uint16_t upper = ConvertLatin1<ToUppercase, false>(c);
    uint16_t test = Latin1::ConvertNonLatin1ToLatin1(c);
    // Filter out all character whose upper is not their lower or vice versa.
    if (lower == 0 && upper == 0) {
      CheckCanonicalEquivalence(c, test);
      continue;
    }
    if (lower > Latin1::kMaxChar && upper > Latin1::kMaxChar) {
      CheckCanonicalEquivalence(c, test);
      continue;
    }
    if (lower == 0 && upper != 0) {
      lower = ConvertLatin1<ToLowercase, false>(upper);
    }
    if (upper == 0 && lower != c) {
      upper = ConvertLatin1<ToUppercase, false>(lower);
    }
    if (lower > Latin1::kMaxChar && upper > Latin1::kMaxChar) {
      CheckCanonicalEquivalence(c, test);
      continue;
    }
    if (upper != c && lower != c) {
      CheckCanonicalEquivalence(c, test);
      continue;
    }
    CHECK_EQ(Min(upper, lower), test);
  }
}
