// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_STRING_SEARCH_H_
#define SRC_STRING_SEARCH_H_

#include "node.h"
#include <string.h>

namespace node {
namespace stringsearch {


// Returns the maximum of the two parameters.
template <typename T>
T Max(T a, T b) {
  return a < b ? b : a;
}


static const uint32_t kMaxOneByteCharCodeU = 0xff;


static inline size_t NonOneByteStart(const uint16_t* chars, size_t length) {
  const uint16_t* limit = chars + length;
  const uint16_t* start = chars;
  while (chars < limit) {
    if (*chars > kMaxOneByteCharCodeU)
      return static_cast<size_t>(chars - start);
    ++chars;
  }
  return static_cast<size_t>(chars - start);
}


static inline bool IsOneByte(const uint16_t* chars, size_t length) {
  return NonOneByteStart(chars, length) >= length;
}


template <typename T>
class Vector {
 public:
  Vector(T* data, size_t length) : start_(data), length_(length) {
    ASSERT(length > 0 && data != nullptr);
  }

  // Returns the length of the vector.
  size_t length() const { return length_; }

  T* start() const { return start_; }

  // Access individual vector elements - checks bounds in debug mode.
  T& operator[](size_t index) const {
    ASSERT(0 <= index && index < length_);
    return start_[index];
  }

  const T& at(size_t index) const { return operator[](index); }

  bool operator==(const Vector<T>& other) const {
    if (length_ != other.length_)
      return false;
    if (start_ == other.start_)
      return true;
    for (size_t i = 0; i < length_; ++i) {
      if (start_[i] != other.start_[i]) {
        return false;
      }
    }
    return true;
  }

 private:
  T* start_;
  size_t length_;
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

  static inline bool IsOneByteString(Vector<const uint8_t> string) {
    return true;
  }

  static inline bool IsOneByteString(Vector<const uint16_t> string) {
    return IsOneByte(string.start(), string.length());
  }
};

template <typename PatternChar, typename SubjectChar>
class StringSearch : private StringSearchBase {
 public:
  explicit StringSearch(Vector<const PatternChar> pattern)
      : pattern_(pattern), start_(0) {
    if (pattern.length() >= kBMMaxShift) {
      start_ = pattern.length() - kBMMaxShift;
    }

    if (sizeof(PatternChar) > sizeof(SubjectChar)) {
      if (!IsOneByteString(pattern_)) {
        strategy_ = &FailSearch;
        return;
      }
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

  size_t Search(Vector<const SubjectChar> subject, size_t index) {
    return strategy_(this, subject, index);
  }

  static inline int AlphabetSize() {
    if (sizeof(PatternChar) == 1) {
      // Latin1 needle.
      return kLatin1AlphabetSize;
    } else {
      // UC16 needle.
      return kUC16AlphabetSize;
    }

    static_assert(sizeof(PatternChar) == sizeof(uint8_t) ||
                      sizeof(PatternChar) == sizeof(uint16_t),
                  "sizeof(PatternChar) == sizeof(uint16_t) || sizeof(uint8_t)");
  }

 private:
  typedef size_t (*SearchFunction)(  // NOLINT - it's not a cast!
      StringSearch<PatternChar, SubjectChar>*,
      Vector<const SubjectChar>,
      size_t);

  static size_t FailSearch(StringSearch<PatternChar, SubjectChar>*,
                           Vector<const SubjectChar> subject,
                           size_t) {
    return subject.length();
  }

  static size_t SingleCharSearch(StringSearch<PatternChar, SubjectChar>* search,
                                 Vector<const SubjectChar> subject,
                                 size_t start_index);

  static size_t LinearSearch(StringSearch<PatternChar, SubjectChar>* search,
                             Vector<const SubjectChar> subject,
                             size_t start_index);

  static size_t InitialSearch(StringSearch<PatternChar, SubjectChar>* search,
                              Vector<const SubjectChar> subject,
                              size_t start_index);

  static size_t BoyerMooreHorspoolSearch(
      StringSearch<PatternChar, SubjectChar>* search,
      Vector<const SubjectChar> subject,
      size_t start_index);

  static size_t BoyerMooreSearch(StringSearch<PatternChar, SubjectChar>* search,
                                 Vector<const SubjectChar> subject,
                                 size_t start_index);

  void PopulateBoyerMooreHorspoolTable();

  void PopulateBoyerMooreTable();

  static inline bool exceedsOneByte(uint8_t c) { return false; }

  static inline bool exceedsOneByte(uint16_t c) {
    return c > kMaxOneByteCharCodeU;
  }

  static inline int CharOccurrence(int* bad_char_occurrence,
                                   SubjectChar char_code) {
    if (sizeof(SubjectChar) == 1) {
      return bad_char_occurrence[static_cast<int>(char_code)];
    }
    if (sizeof(PatternChar) == 1) {
      if (exceedsOneByte(char_code)) {
        return -1;
      }
      return bad_char_occurrence[static_cast<unsigned int>(char_code)];
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
  Vector<const PatternChar> pattern_;
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


template <typename PatternChar, typename SubjectChar>
inline size_t FindFirstCharacter(Vector<const PatternChar> pattern,
                                 Vector<const SubjectChar> subject,
                                 size_t index) {
  const PatternChar pattern_first_char = pattern[0];
  const size_t max_n = (subject.length() - pattern.length() + 1);

  const uint8_t search_byte = GetHighestValueByte(pattern_first_char);
  const SubjectChar search_char = static_cast<SubjectChar>(pattern_first_char);
  size_t pos = index;
  do {
    const SubjectChar* char_pos = reinterpret_cast<const SubjectChar*>(
        memchr(subject.start() + pos, search_byte,
               (max_n - pos) * sizeof(SubjectChar)));
    if (char_pos == nullptr)
      return subject.length();
    char_pos = AlignDown(char_pos, sizeof(SubjectChar));
    pos = static_cast<size_t>(char_pos - subject.start());
    if (subject[pos] == search_char)
      return pos;
  } while (++pos < max_n);

  return subject.length();
}


template <>
inline size_t FindFirstCharacter(Vector<const uint8_t> pattern,
                                 Vector<const uint8_t> subject,
                                 size_t index) {
  const uint8_t pattern_first_char = pattern[0];
  const size_t max_n = (subject.length() - pattern.length() + 1);

  const uint8_t* char_pos = reinterpret_cast<const uint8_t*>(
      memchr(subject.start() + index, pattern_first_char, max_n - index));
  if (char_pos == nullptr)
    return subject.length();
  return static_cast<size_t>(char_pos - subject.start());
}

//---------------------------------------------------------------------
// Single Character Pattern Search Strategy
//---------------------------------------------------------------------

template <typename PatternChar, typename SubjectChar>
size_t StringSearch<PatternChar, SubjectChar>::SingleCharSearch(
    StringSearch<PatternChar, SubjectChar>* search,
    Vector<const SubjectChar> subject,
    size_t index) {
  CHECK_EQ(1, search->pattern_.length());
  PatternChar pattern_first_char = search->pattern_[0];

  if (sizeof(SubjectChar) == 1 && sizeof(PatternChar) == 1) {
    return FindFirstCharacter(search->pattern_, subject, index);
  } else {
    if (sizeof(PatternChar) > sizeof(SubjectChar)) {
      if (exceedsOneByte(pattern_first_char)) {
        return -1;
      }
    }
    return FindFirstCharacter(search->pattern_, subject, index);
  }
}

//---------------------------------------------------------------------
// Linear Search Strategy
//---------------------------------------------------------------------

template <typename PatternChar, typename SubjectChar>
inline bool CharCompare(const PatternChar* pattern,
                        const SubjectChar* subject,
                        size_t length) {
  ASSERT_GT(length, 0);
  size_t pos = 0;
  do {
    if (pattern[pos] != subject[pos]) {
      return false;
    }
    pos++;
  } while (pos < length);
  return true;
}

// Simple linear search for short patterns. Never bails out.
template <typename PatternChar, typename SubjectChar>
size_t StringSearch<PatternChar, SubjectChar>::LinearSearch(
    StringSearch<PatternChar, SubjectChar>* search,
    Vector<const SubjectChar> subject,
    size_t index) {
  Vector<const PatternChar> pattern = search->pattern_;
  CHECK_GT(pattern.length(), 1);
  const size_t pattern_length = pattern.length();
  size_t i = index;
  const size_t n = subject.length() - pattern_length;
  while (i <= n) {
    i = FindFirstCharacter(pattern, subject, i);
    if (i == subject.length())
      return subject.length();
    ASSERT_LE(i, n);
    i++;

    // Loop extracted to separate function to allow using return to do
    // a deeper break.
    if (CharCompare(pattern.start() + 1, subject.start() + i,
                    pattern_length - 1)) {
      return i - 1;
    }
  }
  return subject.length();
}

//---------------------------------------------------------------------
// Boyer-Moore string search
//---------------------------------------------------------------------

template <typename PatternChar, typename SubjectChar>
size_t StringSearch<PatternChar, SubjectChar>::BoyerMooreSearch(
    StringSearch<PatternChar, SubjectChar>* search,
    Vector<const SubjectChar> subject,
    size_t start_index) {
  Vector<const PatternChar> pattern = search->pattern_;
  const size_t subject_length = subject.length();
  const size_t pattern_length = pattern.length();
  // Only preprocess at most kBMMaxShift last characters of pattern.
  size_t start = search->start_;

  int* bad_char_occurence = search->bad_char_table();
  int* good_suffix_shift = search->good_suffix_shift_table();

  PatternChar last_char = pattern[pattern_length - 1];
  size_t index = start_index;
  // Continue search from i.
  while (index <= subject_length - pattern_length) {
    size_t j = pattern_length - 1;
    int c;
    while (last_char != (c = subject[index + j])) {
      int shift = j - CharOccurrence(bad_char_occurence, c);
      index += shift;
      if (index > subject_length - pattern_length) {
        return subject.length();
      }
    }
    while (j >= 0 && pattern[j] == (c = subject[index + j])) {
      if (j == 0) {
        return index;
      }
      j--;
    }
    if (j < start) {
      // we have matched more than our tables allow us to be smart about.
      // Fall back on BMH shift.
      index += pattern_length - 1 -
               CharOccurrence(bad_char_occurence,
                              static_cast<SubjectChar>(last_char));
    } else {
      int gs_shift = good_suffix_shift[j + 1];
      int bc_occ = CharOccurrence(bad_char_occurence, c);
      int shift = j - bc_occ;
      if (gs_shift > shift) {
        shift = gs_shift;
      }
      index += shift;
    }
  }

  return subject.length();
}

template <typename PatternChar, typename SubjectChar>
void StringSearch<PatternChar, SubjectChar>::PopulateBoyerMooreTable() {
  const size_t pattern_length = pattern_.length();
  const PatternChar* pattern = pattern_.start();
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
  PatternChar last_char = pattern[pattern_length - 1];
  size_t suffix = pattern_length + 1;
  {
    size_t i = pattern_length;
    while (i > start) {
      PatternChar c = pattern[i - 1];
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

template <typename PatternChar, typename SubjectChar>
size_t StringSearch<PatternChar, SubjectChar>::BoyerMooreHorspoolSearch(
    StringSearch<PatternChar, SubjectChar>* search,
    Vector<const SubjectChar> subject,
    size_t start_index) {
  Vector<const PatternChar> pattern = search->pattern_;
  const size_t subject_length = subject.length();
  const size_t pattern_length = pattern.length();
  int* char_occurrences = search->bad_char_table();
  int64_t badness = -pattern_length;

  // How bad we are doing without a good-suffix table.
  PatternChar last_char = pattern[pattern_length - 1];
  int last_char_shift =
      pattern_length - 1 -
      CharOccurrence(char_occurrences, static_cast<SubjectChar>(last_char));

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
    while (j >= 0 && pattern[j] == (subject[index + j])) {
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

template <typename PatternChar, typename SubjectChar>
void StringSearch<PatternChar, SubjectChar>::PopulateBoyerMooreHorspoolTable() {
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
    PatternChar c = pattern_[i];
    int bucket = (sizeof(PatternChar) == 1) ? c : c % AlphabetSize();
    bad_char_occurrence[bucket] = i;
  }
}

//---------------------------------------------------------------------
// Linear string search with bailout to BMH.
//---------------------------------------------------------------------

// Simple linear search for short patterns, which bails out if the string
// isn't found very early in the subject. Upgrades to BoyerMooreHorspool.
template <typename PatternChar, typename SubjectChar>
size_t StringSearch<PatternChar, SubjectChar>::InitialSearch(
    StringSearch<PatternChar, SubjectChar>* search,
    Vector<const SubjectChar> subject,
    size_t index) {
  Vector<const PatternChar> pattern = search->pattern_;
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
      ASSERT_LE(i, n);
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
template <typename SubjectChar, typename PatternChar>
size_t SearchString(Vector<const SubjectChar> subject,
                    Vector<const PatternChar> pattern,
                    size_t start_index) {
  StringSearch<PatternChar, SubjectChar> search(pattern);
  return search.Search(subject, start_index);
}
}  // namespace stringsearch
}  // namespace node

namespace node {
using node::stringsearch::Vector;

template <typename SubjectChar, typename PatternChar>
size_t SearchString(const SubjectChar* haystack,
                    size_t haystack_length,
                    const PatternChar* needle,
                    size_t needle_length,
                    size_t start_index) {
  return node::stringsearch::SearchString(
      Vector<const SubjectChar>(haystack, haystack_length),
      Vector<const PatternChar>(needle, needle_length),
      start_index);
}
}  // namespace node

#endif  // SRC_STRING_SEARCH_H_
