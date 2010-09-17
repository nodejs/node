// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_STRING_SEARCH_H_
#define V8_STRING_SEARCH_H_

namespace v8 {
namespace internal {


// Cap on the maximal shift in the Boyer-Moore implementation. By setting a
// limit, we can fix the size of tables. For a needle longer than this limit,
// search will not be optimal, since we only build tables for a smaller suffix
// of the string, which is a safe approximation.
static const int kBMMaxShift = 250;
// Reduce alphabet to this size.
// One of the tables used by Boyer-Moore and Boyer-Moore-Horspool has size
// proportional to the input alphabet. We reduce the alphabet size by
// equating input characters modulo a smaller alphabet size. This gives
// a potentially less efficient searching, but is a safe approximation.
// For needles using only characters in the same Unicode 256-code point page,
// there is no search speed degradation.
static const int kBMAlphabetSize = 256;
// For patterns below this length, the skip length of Boyer-Moore is too short
// to compensate for the algorithmic overhead compared to simple brute force.
static const int kBMMinPatternLength = 7;

// Holds the two buffers used by Boyer-Moore string search's Good Suffix
// shift. Only allows the last kBMMaxShift characters of the needle
// to be indexed.
class BMGoodSuffixBuffers {
 public:
  BMGoodSuffixBuffers() {}
  inline void Initialize(int needle_length) {
    ASSERT(needle_length > 1);
    int start = needle_length < kBMMaxShift ? 0 : needle_length - kBMMaxShift;
    int len = needle_length - start;
    biased_suffixes_ = suffixes_ - start;
    biased_good_suffix_shift_ = good_suffix_shift_ - start;
    for (int i = 0; i <= len; i++) {
      good_suffix_shift_[i] = len;
    }
  }
  inline int& suffix(int index) {
    ASSERT(biased_suffixes_ + index >= suffixes_);
    return biased_suffixes_[index];
  }
  inline int& shift(int index) {
    ASSERT(biased_good_suffix_shift_ + index >= good_suffix_shift_);
    return biased_good_suffix_shift_[index];
  }
 private:
  int suffixes_[kBMMaxShift + 1];
  int good_suffix_shift_[kBMMaxShift + 1];
  int* biased_suffixes_;
  int* biased_good_suffix_shift_;
  DISALLOW_COPY_AND_ASSIGN(BMGoodSuffixBuffers);
};

// buffers reused by BoyerMoore
struct BMBuffers {
 public:
  static int bad_char_occurrence[kBMAlphabetSize];
  static BMGoodSuffixBuffers bmgs_buffers;
};

// State of the string match tables.
// SIMPLE: No usable content in the buffers.
// BOYER_MOORE_HORSPOOL: The bad_char_occurence table has been populated.
// BOYER_MOORE: The bmgs_buffers tables have also been populated.
// Whenever starting with a new needle, one should call InitializeStringSearch
// to determine which search strategy to use, and in the case of a long-needle
// strategy, the call also initializes the algorithm to SIMPLE.
enum StringSearchAlgorithm { SIMPLE_SEARCH, BOYER_MOORE_HORSPOOL, BOYER_MOORE };
static StringSearchAlgorithm algorithm;


// Compute the bad-char table for Boyer-Moore in the static buffer.
template <typename PatternChar>
static void BoyerMoorePopulateBadCharTable(Vector<const PatternChar> pattern) {
  // Only preprocess at most kBMMaxShift last characters of pattern.
  int start = Max(pattern.length() - kBMMaxShift, 0);
  // Run forwards to populate bad_char_table, so that *last* instance
  // of character equivalence class is the one registered.
  // Notice: Doesn't include the last character.
  int table_size = (sizeof(PatternChar) == 1) ? String::kMaxAsciiCharCode + 1
                                        : kBMAlphabetSize;
  if (start == 0) {  // All patterns less than kBMMaxShift in length.
    memset(BMBuffers::bad_char_occurrence,
           -1,
           table_size * sizeof(*BMBuffers::bad_char_occurrence));
  } else {
    for (int i = 0; i < table_size; i++) {
      BMBuffers::bad_char_occurrence[i] = start - 1;
    }
  }
  for (int i = start; i < pattern.length() - 1; i++) {
    PatternChar c = pattern[i];
    int bucket = (sizeof(PatternChar) ==1) ? c : c % kBMAlphabetSize;
    BMBuffers::bad_char_occurrence[bucket] = i;
  }
}


template <typename PatternChar>
static void BoyerMoorePopulateGoodSuffixTable(
    Vector<const PatternChar> pattern) {
  int m = pattern.length();
  int start = m < kBMMaxShift ? 0 : m - kBMMaxShift;
  int len = m - start;
  // Compute Good Suffix tables.
  BMBuffers::bmgs_buffers.Initialize(m);

  BMBuffers::bmgs_buffers.shift(m-1) = 1;
  BMBuffers::bmgs_buffers.suffix(m) = m + 1;
  PatternChar last_char = pattern[m - 1];
  int suffix = m + 1;
  {
    int i = m;
    while (i > start) {
      PatternChar c = pattern[i - 1];
      while (suffix <= m && c != pattern[suffix - 1]) {
        if (BMBuffers::bmgs_buffers.shift(suffix) == len) {
          BMBuffers::bmgs_buffers.shift(suffix) = suffix - i;
        }
        suffix = BMBuffers::bmgs_buffers.suffix(suffix);
      }
      BMBuffers::bmgs_buffers.suffix(--i) = --suffix;
      if (suffix == m) {
        // No suffix to extend, so we check against last_char only.
        while ((i > start) && (pattern[i - 1] != last_char)) {
          if (BMBuffers::bmgs_buffers.shift(m) == len) {
            BMBuffers::bmgs_buffers.shift(m) = m - i;
          }
          BMBuffers::bmgs_buffers.suffix(--i) = m;
        }
        if (i > start) {
          BMBuffers::bmgs_buffers.suffix(--i) = --suffix;
        }
      }
    }
  }
  if (suffix < m) {
    for (int i = start; i <= m; i++) {
      if (BMBuffers::bmgs_buffers.shift(i) == len) {
        BMBuffers::bmgs_buffers.shift(i) = suffix - start;
      }
      if (i == suffix) {
        suffix = BMBuffers::bmgs_buffers.suffix(suffix);
      }
    }
  }
}


template <typename SubjectChar, typename PatternChar>
static inline int CharOccurrence(int char_code) {
  if (sizeof(SubjectChar) == 1) {
    return BMBuffers::bad_char_occurrence[char_code];
  }
  if (sizeof(PatternChar) == 1) {
    if (char_code > String::kMaxAsciiCharCode) {
      return -1;
    }
    return BMBuffers::bad_char_occurrence[char_code];
  }
  return BMBuffers::bad_char_occurrence[char_code % kBMAlphabetSize];
}


// Restricted simplified Boyer-Moore string matching.
// Uses only the bad-shift table of Boyer-Moore and only uses it
// for the character compared to the last character of the needle.
template <typename SubjectChar, typename PatternChar>
static int BoyerMooreHorspool(Vector<const SubjectChar> subject,
                              Vector<const PatternChar> pattern,
                              int start_index,
                              bool* complete) {
  ASSERT(algorithm <= BOYER_MOORE_HORSPOOL);
  int n = subject.length();
  int m = pattern.length();

  int badness = -m;

  // How bad we are doing without a good-suffix table.
  int idx;  // No matches found prior to this index.
  PatternChar last_char = pattern[m - 1];
  int last_char_shift =
      m - 1 - CharOccurrence<SubjectChar, PatternChar>(last_char);
  // Perform search
  for (idx = start_index; idx <= n - m;) {
    int j = m - 1;
    int c;
    while (last_char != (c = subject[idx + j])) {
      int bc_occ = CharOccurrence<SubjectChar, PatternChar>(c);
      int shift = j - bc_occ;
      idx += shift;
      badness += 1 - shift;  // at most zero, so badness cannot increase.
      if (idx > n - m) {
        *complete = true;
        return -1;
      }
    }
    j--;
    while (j >= 0 && pattern[j] == (subject[idx + j])) j--;
    if (j < 0) {
      *complete = true;
      return idx;
    } else {
      idx += last_char_shift;
      // Badness increases by the number of characters we have
      // checked, and decreases by the number of characters we
      // can skip by shifting. It's a measure of how we are doing
      // compared to reading each character exactly once.
      badness += (m - j) - last_char_shift;
      if (badness > 0) {
        *complete = false;
        return idx;
      }
    }
  }
  *complete = true;
  return -1;
}


template <typename SubjectChar, typename PatternChar>
static int BoyerMooreIndexOf(Vector<const SubjectChar> subject,
                             Vector<const PatternChar> pattern,
                             int idx) {
  ASSERT(algorithm <= BOYER_MOORE);
  int n = subject.length();
  int m = pattern.length();
  // Only preprocess at most kBMMaxShift last characters of pattern.
  int start = m < kBMMaxShift ? 0 : m - kBMMaxShift;

  PatternChar last_char = pattern[m - 1];
  // Continue search from i.
  while (idx <= n - m) {
    int j = m - 1;
    SubjectChar c;
    while (last_char != (c = subject[idx + j])) {
      int shift = j - CharOccurrence<SubjectChar, PatternChar>(c);
      idx += shift;
      if (idx > n - m) {
        return -1;
      }
    }
    while (j >= 0 && pattern[j] == (c = subject[idx + j])) j--;
    if (j < 0) {
      return idx;
    } else if (j < start) {
      // we have matched more than our tables allow us to be smart about.
      // Fall back on BMH shift.
      idx += m - 1 - CharOccurrence<SubjectChar, PatternChar>(last_char);
    } else {
      int gs_shift = BMBuffers::bmgs_buffers.shift(j + 1);
      int bc_occ = CharOccurrence<SubjectChar, PatternChar>(c);
      int shift = j - bc_occ;
      if (gs_shift > shift) {
        shift = gs_shift;
      }
      idx += shift;
    }
  }

  return -1;
}


// Trivial string search for shorter strings.
// On return, if "complete" is set to true, the return value is the
// final result of searching for the patter in the subject.
// If "complete" is set to false, the return value is the index where
// further checking should start, i.e., it's guaranteed that the pattern
// does not occur at a position prior to the returned index.
template <typename PatternChar, typename SubjectChar>
static int SimpleIndexOf(Vector<const SubjectChar> subject,
                         Vector<const PatternChar> pattern,
                         int idx,
                         bool* complete) {
  ASSERT(pattern.length() > 1);
  int pattern_length = pattern.length();
  // Badness is a count of how much work we have done.  When we have
  // done enough work we decide it's probably worth switching to a better
  // algorithm.
  int badness = -10 - (pattern_length << 2);

  // We know our pattern is at least 2 characters, we cache the first so
  // the common case of the first character not matching is faster.
  PatternChar pattern_first_char = pattern[0];
  for (int i = idx, n = subject.length() - pattern_length; i <= n; i++) {
    badness++;
    if (badness > 0) {
      *complete = false;
      return i;
    }
    if (sizeof(SubjectChar) == 1 && sizeof(PatternChar) == 1) {
      const SubjectChar* pos = reinterpret_cast<const SubjectChar*>(
          memchr(subject.start() + i,
                 pattern_first_char,
                 n - i + 1));
      if (pos == NULL) {
        *complete = true;
        return -1;
      }
      i = static_cast<int>(pos - subject.start());
    } else {
      if (subject[i] != pattern_first_char) continue;
    }
    int j = 1;
    do {
      if (pattern[j] != subject[i+j]) {
        break;
      }
      j++;
    } while (j < pattern_length);
    if (j == pattern_length) {
      *complete = true;
      return i;
    }
    badness += j;
  }
  *complete = true;
  return -1;
}

// Simple indexOf that never bails out. For short patterns only.
template <typename PatternChar, typename SubjectChar>
static int SimpleIndexOf(Vector<const SubjectChar> subject,
                         Vector<const PatternChar> pattern,
                         int idx) {
  int pattern_length = pattern.length();
  PatternChar pattern_first_char = pattern[0];
  for (int i = idx, n = subject.length() - pattern_length; i <= n; i++) {
    if (sizeof(SubjectChar) == 1 && sizeof(PatternChar) == 1) {
      const SubjectChar* pos = reinterpret_cast<const SubjectChar*>(
          memchr(subject.start() + i,
                 pattern_first_char,
                 n - i + 1));
      if (pos == NULL) return -1;
      i = static_cast<int>(pos - subject.start());
    } else {
      if (subject[i] != pattern_first_char) continue;
    }
    int j = 1;
    while (j < pattern_length) {
      if (pattern[j] != subject[i+j]) {
        break;
      }
      j++;
    }
    if (j == pattern_length) {
      return i;
    }
  }
  return -1;
}


// Strategy for searching for a string in another string.
enum StringSearchStrategy { SEARCH_FAIL, SEARCH_SHORT, SEARCH_LONG };


template <typename PatternChar>
static inline StringSearchStrategy InitializeStringSearch(
    Vector<const PatternChar> pat, bool ascii_subject) {
  // We have an ASCII haystack and a non-ASCII needle. Check if there
  // really is a non-ASCII character in the needle and bail out if there
  // is.
  if (ascii_subject && sizeof(PatternChar) > 1) {
    for (int i = 0; i < pat.length(); i++) {
      uc16 c = pat[i];
      if (c > String::kMaxAsciiCharCode) {
        return SEARCH_FAIL;
      }
    }
  }
  if (pat.length() < kBMMinPatternLength) {
    return SEARCH_SHORT;
  }
  algorithm = SIMPLE_SEARCH;
  return SEARCH_LONG;
}


// Dispatch long needle searches to different algorithms.
template <typename SubjectChar, typename PatternChar>
static int ComplexIndexOf(Vector<const SubjectChar> sub,
                          Vector<const PatternChar> pat,
                          int start_index) {
  ASSERT(pat.length() >= kBMMinPatternLength);
  // Try algorithms in order of increasing setup cost and expected performance.
  bool complete;
  int idx = start_index;
  switch (algorithm) {
    case SIMPLE_SEARCH:
      idx = SimpleIndexOf(sub, pat, idx, &complete);
      if (complete) return idx;
      BoyerMoorePopulateBadCharTable(pat);
      algorithm = BOYER_MOORE_HORSPOOL;
      // FALLTHROUGH.
    case BOYER_MOORE_HORSPOOL:
      idx = BoyerMooreHorspool(sub, pat, idx, &complete);
      if (complete) return idx;
      // Build the Good Suffix table and continue searching.
      BoyerMoorePopulateGoodSuffixTable(pat);
      algorithm = BOYER_MOORE;
      // FALLTHROUGH.
    case BOYER_MOORE:
      return BoyerMooreIndexOf(sub, pat, idx);
  }
  UNREACHABLE();
  return -1;
}


// Dispatch to different search strategies for a single search.
// If searching multiple times on the same needle, the search
// strategy should only be computed once and then dispatch to different
// loops.
template <typename SubjectChar, typename PatternChar>
static int StringSearch(Vector<const SubjectChar> sub,
                        Vector<const PatternChar> pat,
                        int start_index) {
  bool ascii_subject = (sizeof(SubjectChar) == 1);
  StringSearchStrategy strategy = InitializeStringSearch(pat, ascii_subject);
  switch (strategy) {
    case SEARCH_FAIL: return -1;
    case SEARCH_SHORT: return SimpleIndexOf(sub, pat, start_index);
    case SEARCH_LONG: return ComplexIndexOf(sub, pat, start_index);
  }
  UNREACHABLE();
  return -1;
}

}}  // namespace v8::internal

#endif  // V8_STRING_SEARCH_H_
