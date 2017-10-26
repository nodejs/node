// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_STRING_SEARCH_H_
#define SRC_STRING_SEARCH_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_internals.h"
#include <string.h>

namespace node {
namespace stringsearch {


// Returns the maximum of the two parameters.
template <typename T>
T Max(T a, T b) {
  return a < b ? b : a;
}


static const uint32_t kMaxOneByteCharCodeU = 0xff;

template <typename T>
class Vector {
 public:
  Vector(T* data, size_t length, bool isForward)
      : start_(data), length_(length), is_forward_(isForward) {
    CHECK(length > 0 && data != nullptr);
  }

  // Returns the start of the memory range.
  // For vector v this is NOT necessarily &v[0], see forward().
  const T* start() const { return start_; }

  // Returns the length of the vector, in characters.
  size_t length() const { return length_; }

  // Returns true if the Vector is front-to-back, false if back-to-front.
  // In the latter case, v[0] corresponds to the *end* of the memory range.
  size_t forward() const { return is_forward_; }

  // Access individual vector elements - checks bounds in debug mode.
  T& operator[](size_t index) const {
#ifdef DEBUG
    CHECK(index < length_);
#endif
    return start_[is_forward_ ? index : (length_ - index - 1)];
  }

 private:
  T* start_;
  size_t length_;
  bool is_forward_;
};


//---------------------------------------------------------------------
// String Search object.
//---------------------------------------------------------------------

// Class holding constants and methods that apply to all string search variants,
// independently of subject and pattern char size.
class StringSearchBase {
 protected:
  // Cap on the maximal shift in the Boyer-Moore implementation. By setting a
  // limit, we can fix the size of tables. For a needle longer than this limit,
  // search will not be optimal, since we only build tables for a suffix
  // of the string, but it is a safe approximation.
  static const int kBMMaxShift = 250;

  // Reduce alphabet to this size.
  // One of the tables used by Boyer-Moore and Boyer-Moore-Horspool has size
  // proportional to the input alphabet. We reduce the alphabet size by
  // equating input characters modulo a smaller alphabet size. This gives
  // a potentially less efficient searching, but is a safe approximation.
  // For needles using only characters in the same Unicode 256-code point page,
  // there is no search speed degradation.
  static const int kLatin1AlphabetSize = 256;
  static const int kUC16AlphabetSize = 256;

  // Bad-char shift table stored in the state. It's length is the alphabet size.
  // For patterns below this length, the skip length of Boyer-Moore is too short
  // to compensate for the algorithmic overhead compared to simple brute force.
  static const int kBMMinPatternLength = 8;

  // Store for the BoyerMoore(Horspool) bad char shift table.
  static int kBadCharShiftTable[kUC16AlphabetSize];
  // Store for the BoyerMoore good suffix shift table.
  static int kGoodSuffixShiftTable[kBMMaxShift + 1];
  // Table used temporarily while building the BoyerMoore good suffix
  // shift table.
  static int kSuffixTable[kBMMaxShift + 1];
};

template <typename Char>
class StringSearch : private StringSearchBase {
 public:
  explicit StringSearch(Vector<const Char> pattern)
      : pattern_(pattern), start_(0) {
    if (pattern.length() >= kBMMaxShift) {
      start_ = pattern.length() - kBMMaxShift;
    }

    size_t pattern_length = pattern_.length();
    CHECK_GT(pattern_length, 0);
    if (pattern_length < kBMMinPatternLength) {
      if (pattern_length == 1) {
        strategy_ = &SingleCharSearch;
        return;
      }
      strategy_ = &LinearSearch;
      return;
    }
    strategy_ = &InitialSearch;
  }

  size_t Search(Vector<const Char> subject, size_t index) {
    return strategy_(this, subject, index);
  }

  static inline int AlphabetSize() {
    if (sizeof(Char) == 1) {
      // Latin1 needle.
      return kLatin1AlphabetSize;
    } else {
      // UC16 needle.
      return kUC16AlphabetSize;
    }

    static_assert(sizeof(Char) == sizeof(uint8_t) ||
                  sizeof(Char) == sizeof(uint16_t),
                  "sizeof(Char) == sizeof(uint16_t) || sizeof(uint8_t)");
  }

 private:
  typedef size_t (*SearchFunction)(
      StringSearch<Char>*,
      Vector<const Char>,
      size_t);

  static size_t SingleCharSearch(StringSearch<Char>* search,
                                 Vector<const Char> subject,
                                 size_t start_index);

  static size_t LinearSearch(StringSearch<Char>* search,
                             Vector<const Char> subject,
                             size_t start_index);

  static size_t InitialSearch(StringSearch<Char>* search,
                              Vector<const Char> subject,
                              size_t start_index);

  static size_t BoyerMooreHorspoolSearch(
      StringSearch<Char>* search,
      Vector<const Char> subject,
      size_t start_index);

  static size_t BoyerMooreSearch(StringSearch<Char>* search,
                                 Vector<const Char> subject,
                                 size_t start_index);

  void PopulateBoyerMooreHorspoolTable();

  void PopulateBoyerMooreTable();

  static inline int CharOccurrence(int* bad_char_occurrence,
                                   Char char_code) {
    if (sizeof(Char) == 1) {
      return bad_char_occurrence[static_cast<int>(char_code)];
    }
    // Both pattern and subject are UC16. Reduce character to equivalence class.
    int equiv_class = char_code % kUC16AlphabetSize;
    return bad_char_occurrence[equiv_class];
  }

  // Store for the BoyerMoore(Horspool) bad char shift table.
  // Return a table covering the last kBMMaxShift+1 positions of
  // pattern.
  int* bad_char_table() { return kBadCharShiftTable; }

  // Store for the BoyerMoore good suffix shift table.
  int* good_suffix_shift_table() {
    // Return biased pointer that maps the range  [start_..pattern_.length()
    // to the kGoodSuffixShiftTable array.
    return kGoodSuffixShiftTable - start_;
  }

  // Table used temporarily while building the BoyerMoore good suffix
  // shift table.
  int* suffix_table() {
    // Return biased pointer that maps the range  [start_..pattern_.length()
    // to the kSuffixTable array.
    return kSuffixTable - start_;
  }

  // The pattern to search for.
  Vector<const Char> pattern_;
  // Pointer to implementation of the search.
  SearchFunction strategy_;
  // Cache value of Max(0, pattern_length() - kBMMaxShift)
  size_t start_;
};


template <typename T, typename U>
inline T AlignDown(T value, U alignment) {
  return reinterpret_cast<T>(
      (reinterpret_cast<uintptr_t>(value) & ~(alignment - 1)));
}


inline uint8_t GetHighestValueByte(uint16_t character) {
  return Max(static_cast<uint8_t>(character & 0xFF),
             static_cast<uint8_t>(character >> 8));
}


inline uint8_t GetHighestValueByte(uint8_t character) { return character; }


// Searches for a byte value in a memory buffer, back to front.
// Uses memrchr(3) on systems which support it, for speed.
// Falls back to a vanilla for loop on non-GNU systems such as Windows.
inline const void* MemrchrFill(const void* haystack, uint8_t needle,
                               size_t haystack_len) {
#ifdef _GNU_SOURCE
  return memrchr(haystack, needle, haystack_len);
#else
  const uint8_t* haystack8 = static_cast<const uint8_t*>(haystack);
  for (size_t i = haystack_len - 1; i != static_cast<size_t>(-1); i--) {
    if (haystack8[i] == needle) {
      return haystack8 + i;
    }
  }
  return nullptr;
#endif
}


// Finds the first occurrence of *two-byte* character pattern[0] in the string
// `subject`. Does not check that the whole pattern matches.
template <typename Char>
inline size_t FindFirstCharacter(Vector<const Char> pattern,
                                 Vector<const Char> subject, size_t index) {
  const Char pattern_first_char = pattern[0];
  const size_t max_n = (subject.length() - pattern.length() + 1);

  // For speed, search for the more `rare` of the two bytes in pattern[0]
  // using memchr / memrchr (which are much faster than a simple for loop).
  const uint8_t search_byte = GetHighestValueByte(pattern_first_char);
  size_t pos = index;
  do {
    const size_t bytes_to_search = (max_n - pos) * sizeof(Char);
    const void* void_pos;
    if (subject.forward()) {
      // Assert that bytes_to_search won't overflow
      CHECK_LE(pos, max_n);
      CHECK_LE(max_n - pos, SIZE_MAX / sizeof(Char));
      void_pos = memchr(subject.start() + pos, search_byte, bytes_to_search);
    } else {
      CHECK_LE(pos, subject.length());
      CHECK_LE(subject.length() - pos, SIZE_MAX / sizeof(Char));
      void_pos = MemrchrFill(subject.start() + pattern.length() - 1,
                             search_byte,
                             bytes_to_search);
    }
    const Char* char_pos = static_cast<const Char*>(void_pos);
    if (char_pos == nullptr)
      return subject.length();

    // Then, for each match, verify that the full two bytes match pattern[0].
    char_pos = AlignDown(char_pos, sizeof(Char));
    size_t raw_pos = static_cast<size_t>(char_pos - subject.start());
    pos = subject.forward() ? raw_pos : (subject.length() - raw_pos - 1);
    if (subject[pos] == pattern_first_char) {
      // Match found, hooray.
      return pos;
    }
    // Search byte matched, but the other byte of pattern[0] didn't. Keep going.
  } while (++pos < max_n);

  return subject.length();
}


// Finds the first occurrence of the byte pattern[0] in string `subject`.
// Does not verify that the whole pattern matches.
template <>
inline size_t FindFirstCharacter(Vector<const uint8_t> pattern,
                                 Vector<const uint8_t> subject,
                                 size_t index) {
  const uint8_t pattern_first_char = pattern[0];
  const size_t subj_len = subject.length();
  const size_t max_n = (subject.length() - pattern.length() + 1);

  const void* pos;
  if (subject.forward()) {
    pos = memchr(subject.start() + index, pattern_first_char, max_n - index);
  } else {
    pos = MemrchrFill(subject.start() + pattern.length() - 1,
                      pattern_first_char,
                      max_n - index);
  }
  const uint8_t* char_pos = static_cast<const uint8_t*>(pos);
  if (char_pos == nullptr) {
    return subj_len;
  }

  size_t raw_pos = static_cast<size_t>(char_pos - subject.start());
  return subject.forward() ? raw_pos : (subj_len - raw_pos - 1);
}

//---------------------------------------------------------------------
// Single Character Pattern Search Strategy
//---------------------------------------------------------------------

template <typename Char>
size_t StringSearch<Char>::SingleCharSearch(
    StringSearch<Char>* search,
    Vector<const Char> subject,
    size_t index) {
  CHECK_EQ(1, search->pattern_.length());
  return FindFirstCharacter(search->pattern_, subject, index);
}

//---------------------------------------------------------------------
// Linear Search Strategy
//---------------------------------------------------------------------

// Simple linear search for short patterns. Never bails out.
template <typename Char>
size_t StringSearch<Char>::LinearSearch(
    StringSearch<Char>* search,
    Vector<const Char> subject,
    size_t index) {
  Vector<const Char> pattern = search->pattern_;
  CHECK_GT(pattern.length(), 1);
  const size_t pattern_length = pattern.length();
  const size_t n = subject.length() - pattern_length;
  for (size_t i = index; i <= n; i++) {
    i = FindFirstCharacter(pattern, subject, i);
    if (i == subject.length())
      return subject.length();
    CHECK_LE(i, n);

    bool matches = true;
    for (size_t j = 1; j < pattern_length; j++) {
      if (pattern[j] != subject[i + j]) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return i;
    }
  }
  return subject.length();
}

//---------------------------------------------------------------------
// Boyer-Moore string search
//---------------------------------------------------------------------

template <typename Char>
size_t StringSearch<Char>::BoyerMooreSearch(
    StringSearch<Char>* search,
    Vector<const Char> subject,
    size_t start_index) {
  Vector<const Char> pattern = search->pattern_;
  const size_t subject_length = subject.length();
  const size_t pattern_length = pattern.length();
  // Only preprocess at most kBMMaxShift last characters of pattern.
  size_t start = search->start_;

  int* bad_char_occurrence = search->bad_char_table();
  int* good_suffix_shift = search->good_suffix_shift_table();

  Char last_char = pattern[pattern_length - 1];
  size_t index = start_index;
  // Continue search from i.
  while (index <= subject_length - pattern_length) {
    size_t j = pattern_length - 1;
    int c;
    while (last_char != (c = subject[index + j])) {
      int shift = j - CharOccurrence(bad_char_occurrence, c);
      index += shift;
      if (index > subject_length - pattern_length) {
        return subject.length();
      }
    }
    while (pattern[j] == (c = subject[index + j])) {
      if (j == 0) {
        return index;
      }
      j--;
    }
    if (j < start) {
      // we have matched more than our tables allow us to be smart about.
      // Fall back on BMH shift.
      index += pattern_length - 1 -
               CharOccurrence(bad_char_occurrence,
                              static_cast<Char>(last_char));
    } else {
      int gs_shift = good_suffix_shift[j + 1];
      int bc_occ = CharOccurrence(bad_char_occurrence, c);
      int shift = j - bc_occ;
      if (gs_shift > shift) {
        shift = gs_shift;
      }
      index += shift;
    }
  }

  return subject.length();
}

template <typename Char>
void StringSearch<Char>::PopulateBoyerMooreTable() {
  const size_t pattern_length = pattern_.length();
  Vector<const Char> pattern = pattern_;
  // Only look at the last kBMMaxShift characters of pattern (from start_
  // to pattern_length).
  const size_t start = start_;
  const size_t length = pattern_length - start;

  // Biased tables so that we can use pattern indices as table indices,
  // even if we only cover the part of the pattern from offset start.
  int* shift_table = good_suffix_shift_table();
  int* suffix_table = this->suffix_table();

  // Initialize table.
  for (size_t i = start; i < pattern_length; i++) {
    shift_table[i] = length;
  }
  shift_table[pattern_length] = 1;
  suffix_table[pattern_length] = pattern_length + 1;

  if (pattern_length <= start) {
    return;
  }

  // Find suffixes.
  Char last_char = pattern_[pattern_length - 1];
  size_t suffix = pattern_length + 1;
  {
    size_t i = pattern_length;
    while (i > start) {
      Char c = pattern[i - 1];
      while (suffix <= pattern_length && c != pattern[suffix - 1]) {
        if (static_cast<size_t>(shift_table[suffix]) == length) {
          shift_table[suffix] = suffix - i;
        }
        suffix = suffix_table[suffix];
      }
      suffix_table[--i] = --suffix;
      if (suffix == pattern_length) {
        // No suffix to extend, so we check against last_char only.
        while ((i > start) && (pattern[i - 1] != last_char)) {
          if (static_cast<size_t>(shift_table[pattern_length]) == length) {
            shift_table[pattern_length] = pattern_length - i;
          }
          suffix_table[--i] = pattern_length;
        }
        if (i > start) {
          suffix_table[--i] = --suffix;
        }
      }
    }
  }
  // Build shift table using suffixes.
  if (suffix < pattern_length) {
    for (size_t i = start; i <= pattern_length; i++) {
      if (static_cast<size_t>(shift_table[i]) == length) {
        shift_table[i] = suffix - start;
      }
      if (i == suffix) {
        suffix = suffix_table[suffix];
      }
    }
  }
}

//---------------------------------------------------------------------
// Boyer-Moore-Horspool string search.
//---------------------------------------------------------------------

template <typename Char>
size_t StringSearch<Char>::BoyerMooreHorspoolSearch(
    StringSearch<Char>* search,
    Vector<const Char> subject,
    size_t start_index) {
  Vector<const Char> pattern = search->pattern_;
  const size_t subject_length = subject.length();
  const size_t pattern_length = pattern.length();
  int* char_occurrences = search->bad_char_table();
  int64_t badness = -pattern_length;

  // How bad we are doing without a good-suffix table.
  Char last_char = pattern[pattern_length - 1];
  int last_char_shift =
      pattern_length - 1 -
      CharOccurrence(char_occurrences, static_cast<Char>(last_char));

  // Perform search
  size_t index = start_index;  // No matches found prior to this index.
  while (index <= subject_length - pattern_length) {
    size_t j = pattern_length - 1;
    int subject_char;
    while (last_char != (subject_char = subject[index + j])) {
      int bc_occ = CharOccurrence(char_occurrences, subject_char);
      int shift = j - bc_occ;
      index += shift;
      badness += 1 - shift;  // at most zero, so badness cannot increase.
      if (index > subject_length - pattern_length) {
        return subject_length;
      }
    }
    j--;
    while (pattern[j] == (subject[index + j])) {
      if (j == 0) {
        return index;
      }
      j--;
    }
    index += last_char_shift;
    // Badness increases by the number of characters we have
    // checked, and decreases by the number of characters we
    // can skip by shifting. It's a measure of how we are doing
    // compared to reading each character exactly once.
    badness += (pattern_length - j) - last_char_shift;
    if (badness > 0) {
      search->PopulateBoyerMooreTable();
      search->strategy_ = &BoyerMooreSearch;
      return BoyerMooreSearch(search, subject, index);
    }
  }
  return subject.length();
}

template <typename Char>
void StringSearch<Char>::PopulateBoyerMooreHorspoolTable() {
  const size_t pattern_length = pattern_.length();

  int* bad_char_occurrence = bad_char_table();

  // Only preprocess at most kBMMaxShift last characters of pattern.
  const size_t start = start_;
  // Run forwards to populate bad_char_table, so that *last* instance
  // of character equivalence class is the one registered.
  // Notice: Doesn't include the last character.
  const size_t table_size = AlphabetSize();
  if (start == 0) {
    // All patterns less than kBMMaxShift in length.
    memset(bad_char_occurrence, -1, table_size * sizeof(*bad_char_occurrence));
  } else {
    for (size_t i = 0; i < table_size; i++) {
      bad_char_occurrence[i] = start - 1;
    }
  }
  for (size_t i = start; i < pattern_length - 1; i++) {
    Char c = pattern_[i];
    int bucket = (sizeof(Char) == 1) ? c : c % AlphabetSize();
    bad_char_occurrence[bucket] = i;
  }
}

//---------------------------------------------------------------------
// Linear string search with bailout to BMH.
//---------------------------------------------------------------------

// Simple linear search for short patterns, which bails out if the string
// isn't found very early in the subject. Upgrades to BoyerMooreHorspool.
template <typename Char>
size_t StringSearch<Char>::InitialSearch(
    StringSearch<Char>* search,
    Vector<const Char> subject,
    size_t index) {
  Vector<const Char> pattern = search->pattern_;
  const size_t pattern_length = pattern.length();
  // Badness is a count of how much work we have done.  When we have
  // done enough work we decide it's probably worth switching to a better
  // algorithm.
  int64_t badness = -10 - (pattern_length << 2);

  // We know our pattern is at least 2 characters, we cache the first so
  // the common case of the first character not matching is faster.
  for (size_t i = index, n = subject.length() - pattern_length; i <= n; i++) {
    badness++;
    if (badness <= 0) {
      i = FindFirstCharacter(pattern, subject, i);
      if (i == subject.length())
        return subject.length();
      CHECK_LE(i, n);
      size_t j = 1;
      do {
        if (pattern[j] != subject[i + j]) {
          break;
        }
        j++;
      } while (j < pattern_length);
      if (j == pattern_length) {
        return i;
      }
      badness += j;
    } else {
      search->PopulateBoyerMooreHorspoolTable();
      search->strategy_ = &BoyerMooreHorspoolSearch;
      return BoyerMooreHorspoolSearch(search, subject, i);
    }
  }
  return subject.length();
}

// Perform a a single stand-alone search.
// If searching multiple times for the same pattern, a search
// object should be constructed once and the Search function then called
// for each search.
template <typename Char>
size_t SearchString(Vector<const Char> subject,
                    Vector<const Char> pattern,
                    size_t start_index) {
  StringSearch<Char> search(pattern);
  return search.Search(subject, start_index);
}
}  // namespace stringsearch
}  // namespace node

namespace node {
using node::stringsearch::Vector;

template <typename Char>
size_t SearchString(const Char* haystack,
                    size_t haystack_length,
                    const Char* needle,
                    size_t needle_length,
                    size_t start_index,
                    bool is_forward) {
  // To do a reverse search (lastIndexOf instead of indexOf) without redundant
  // code, create two vectors that are reversed views into the input strings.
  // For example, v_needle[0] would return the *last* character of the needle.
  // So we're searching for the first instance of rev(needle) in rev(haystack)
  Vector<const Char> v_needle = Vector<const Char>(
      needle, needle_length, is_forward);
  Vector<const Char> v_haystack = Vector<const Char>(
      haystack, haystack_length, is_forward);
  CHECK(haystack_length >= needle_length);
  size_t diff = haystack_length - needle_length;
  size_t relative_start_index;
  if (is_forward) {
    relative_start_index = start_index;
  } else if (diff < start_index) {
    relative_start_index = 0;
  } else {
    relative_start_index = diff - start_index;
  }
  size_t pos = node::stringsearch::SearchString(
      v_haystack, v_needle, relative_start_index);
  if (pos == haystack_length) {
    // not found
    return pos;
  }
  return is_forward ? pos : (haystack_length - needle_length - pos);
}
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_STRING_SEARCH_H_
