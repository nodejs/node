// Copyright 2007-2008 the V8 project authors. All rights reserved.
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

#ifdef ENABLE_DEBUGGER_SUPPORT

#include <stdlib.h>

#include "v8.h"

#include "liveedit.h"
#include "cctest.h"


using namespace v8::internal;

// Anonymous namespace.
namespace {

class StringCompareInput : public Comparator::Input {
 public:
  StringCompareInput(const char* s1, const char* s2) : s1_(s1), s2_(s2) {
  }
  int getLength1() {
    return StrLength(s1_);
  }
  int getLength2() {
    return StrLength(s2_);
  }
  bool equals(int index1, int index2) {
    return s1_[index1] == s2_[index2];
  }

 private:
  const char* s1_;
  const char* s2_;
};


class DiffChunkStruct : public ZoneObject {
 public:
  DiffChunkStruct(int pos1_param, int pos2_param,
                  int len1_param, int len2_param)
      : pos1(pos1_param), pos2(pos2_param),
        len1(len1_param), len2(len2_param), next(NULL) {}
  int pos1;
  int pos2;
  int len1;
  int len2;
  DiffChunkStruct* next;
};


class ListDiffOutputWriter : public Comparator::Output {
 public:
  explicit ListDiffOutputWriter(DiffChunkStruct** next_chunk_pointer)
      : next_chunk_pointer_(next_chunk_pointer) {
    (*next_chunk_pointer_) = NULL;
  }
  void AddChunk(int pos1, int pos2, int len1, int len2) {
    current_chunk_ = new DiffChunkStruct(pos1, pos2, len1, len2);
    (*next_chunk_pointer_) = current_chunk_;
    next_chunk_pointer_ = &current_chunk_->next;
  }
 private:
  DiffChunkStruct** next_chunk_pointer_;
  DiffChunkStruct* current_chunk_;
};


void CompareStringsOneWay(const char* s1, const char* s2,
                          int expected_diff_parameter = -1) {
  StringCompareInput input(s1, s2);

  ZoneScope zone_scope(DELETE_ON_EXIT);

  DiffChunkStruct* first_chunk;
  ListDiffOutputWriter writer(&first_chunk);

  Comparator::CalculateDifference(&input, &writer);

  int len1 = StrLength(s1);
  int len2 = StrLength(s2);

  int pos1 = 0;
  int pos2 = 0;

  int diff_parameter = 0;

  for (DiffChunkStruct* chunk = first_chunk;
      chunk != NULL;
      chunk = chunk->next) {
    int diff_pos1 = chunk->pos1;
    int similar_part_length = diff_pos1 - pos1;
    int diff_pos2 = pos2 + similar_part_length;

    ASSERT_EQ(diff_pos2, chunk->pos2);

    for (int j = 0; j < similar_part_length; j++) {
      ASSERT(pos1 + j < len1);
      ASSERT(pos2 + j < len2);
      ASSERT_EQ(s1[pos1 + j], s2[pos2 + j]);
    }
    diff_parameter += chunk->len1 + chunk->len2;
    pos1 = diff_pos1 + chunk->len1;
    pos2 = diff_pos2 + chunk->len2;
  }
  {
    // After last chunk.
    int similar_part_length = len1 - pos1;
    ASSERT_EQ(similar_part_length, len2 - pos2);
    USE(len2);
    for (int j = 0; j < similar_part_length; j++) {
      ASSERT(pos1 + j < len1);
      ASSERT(pos2 + j < len2);
      ASSERT_EQ(s1[pos1 + j], s2[pos2 + j]);
    }
  }

  if (expected_diff_parameter != -1) {
    ASSERT_EQ(expected_diff_parameter, diff_parameter);
  }
}


void CompareStrings(const char* s1, const char* s2,
                    int expected_diff_parameter = -1) {
  CompareStringsOneWay(s1, s2, expected_diff_parameter);
  CompareStringsOneWay(s2, s1, expected_diff_parameter);
}

}  // Anonymous namespace.


// --- T h e   A c t u a l   T e s t s

TEST(LiveEditDiffer) {
  CompareStrings("zz1zzz12zz123zzz", "zzzzzzzzzz", 6);
  CompareStrings("zz1zzz12zz123zzz", "zz0zzz0zz0zzz", 9);
  CompareStrings("123456789", "987654321", 16);
  CompareStrings("zzz", "yyy", 6);
  CompareStrings("zzz", "zzz12", 2);
  CompareStrings("zzz", "21zzz", 2);
  CompareStrings("cat", "cut", 2);
  CompareStrings("ct", "cut", 1);
  CompareStrings("cat", "ct", 1);
  CompareStrings("cat", "cat", 0);
  CompareStrings("", "", 0);
  CompareStrings("cat", "", 3);
  CompareStrings("a cat", "a capybara", 7);
  CompareStrings("abbabababababaaabbabababababbabbbbbbbababa",
                 "bbbbabababbbabababbbabababababbabbababa");
}

#endif  // ENABLE_DEBUGGER_SUPPORT
