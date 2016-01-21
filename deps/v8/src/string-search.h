// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRING_SEARCH_H_
#define V8_STRING_SEARCH_H_

#include "src/isolate.h"
#include "src/vector.h"

namespace v8 {
namespace internal {


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
  static const int kBMMaxShift = Isolate::kBMMaxShift;

  // Reduce alphabet to this size.
  // One of the tables used by Boyer-Moore and Boyer-Moore-Horspool has size
  // proportional to the input alphabet. We reduce the alphabet size by
  // equating input characters modulo a smaller alphabet size. This gives
  // a potentially less efficient searching, but is a safe approximation.
  // For needles using only characters in the same Unicode 256-code point page,
  // there is no search speed degradation.
  static const int kLatin1AlphabetSize = 256;
  static const int kUC16AlphabetSize = Isolate::kUC16AlphabetSize;

  // Bad-char shift table stored in the state. It's length is the alphabet size.
  // For patterns below this length, the skip length of Boyer-Moore is too short
  // to compensate for the algorithmic overhead compared to simple brute force.
  static const int kBMMinPatternLength = 7;

  static inline bool IsOneByteString(Vector<const uint8_t> string) {
    return true;
  }

  static inline bool IsOneByteString(Vector<const uc16> string) {
    return String::IsOneByte(string.start(), string.length());
  }

  friend class Isolate;
};


template <typename PatternChar, typename SubjectChar>
class StringSearch : private StringSearchBase {
 public:
  StringSearch(Isolate* isolate, Vector<const PatternChar> pattern)
      : isolate_(isolate),
        pattern_(pattern),
        start_(Max(0, pattern.length() - kBMMaxShift)) {
    if (sizeof(PatternChar) > sizeof(SubjectChar)) {
      if (!IsOneByteString(pattern_)) {
        strategy_ = &FailSearch;
        return;
      }
    }
    int pattern_length = pattern_.length();
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

  int Search(Vector<const SubjectChar> subject, int index) {
    return strategy_(this, subject, index);
  }

  static inline int AlphabetSize() {
    if (sizeof(PatternChar) == 1) {
      // Latin1 needle.
      return kLatin1AlphabetSize;
    } else {
      DCHECK(sizeof(PatternChar) == 2);
      // UC16 needle.
      return kUC16AlphabetSize;
    }
  }

 private:
  typedef int (*SearchFunction)(  // NOLINT - it's not a cast!
      StringSearch<PatternChar, SubjectChar>*,
      Vector<const SubjectChar>,
      int);

  static int FailSearch(StringSearch<PatternChar, SubjectChar>*,
                        Vector<const SubjectChar>,
                        int) {
    return -1;
  }

  static int SingleCharSearch(StringSearch<PatternChar, SubjectChar>* search,
                              Vector<const SubjectChar> subject,
                              int start_index);

  static int LinearSearch(StringSearch<PatternChar, SubjectChar>* search,
                          Vector<const SubjectChar> subject,
                          int start_index);

  static int InitialSearch(StringSearch<PatternChar, SubjectChar>* search,
                           Vector<const SubjectChar> subject,
                           int start_index);

  static int BoyerMooreHorspoolSearch(
      StringSearch<PatternChar, SubjectChar>* search,
      Vector<const SubjectChar> subject,
      int start_index);

  static int BoyerMooreSearch(StringSearch<PatternChar, SubjectChar>* search,
                              Vector<const SubjectChar> subject,
                              int start_index);

  void PopulateBoyerMooreHorspoolTable();

  void PopulateBoyerMooreTable();

  static inline bool exceedsOneByte(uint8_t c) {
    return false;
  }

  static inline bool exceedsOneByte(uint16_t c) {
    return c > String::kMaxOneByteCharCodeU;
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

  // The following tables are shared by all searches.
  // TODO(lrn): Introduce a way for a pattern to keep its tables
  // between searches (e.g., for an Atom RegExp).

  // Store for the BoyerMoore(Horspool) bad char shift table.
  // Return a table covering the last kBMMaxShift+1 positions of
  // pattern.
  int* bad_char_table() {
    return isolate_->bad_char_shift_table();
  }

  // Store for the BoyerMoore good suffix shift table.
  int* good_suffix_shift_table() {
    // Return biased pointer that maps the range  [start_..pattern_.length()
    // to the kGoodSuffixShiftTable array.
    return isolate_->good_suffix_shift_table() - start_;
  }

  // Table used temporarily while building the BoyerMoore good suffix
  // shift table.
  int* suffix_table() {
    // Return biased pointer that maps the range  [start_..pattern_.length()
    // to the kSuffixTable array.
    return isolate_->suffix_table() - start_;
  }

  Isolate* isolate_;
  // The pattern to search for.
  Vector<const PatternChar> pattern_;
  // Pointer to implementation of the search.
  SearchFunction strategy_;
  // Cache value of Max(0, pattern_length() - kBMMaxShift)
  int start_;
};


template <typename T, typename U>
inline T AlignDown(T value, U alignment) {
  return reinterpret_cast<T>(
      (reinterpret_cast<uintptr_t>(value) & ~(alignment - 1)));
}


inline uint8_t GetHighestValueByte(uc16 character) {
  return Max(static_cast<uint8_t>(character & 0xFF),
             static_cast<uint8_t>(character >> 8));
}


inline uint8_t GetHighestValueByte(uint8_t character) { return character; }


template <typename PatternChar, typename SubjectChar>
inline int FindFirstCharacter(Vector<const PatternChar> pattern,
                              Vector<const SubjectChar> subject, int index) {
  const PatternChar pattern_first_char = pattern[0];
  const int max_n = (subject.length() - pattern.length() + 1);

  const uint8_t search_byte = GetHighestValueByte(pattern_first_char);
  const SubjectChar search_char = static_cast<SubjectChar>(pattern_first_char);
  int pos = index;
  do {
    DCHECK_GE(max_n - pos, 0);
    const SubjectChar* char_pos = reinterpret_cast<const SubjectChar*>(
        memchr(subject.start() + pos, search_byte,
               (max_n - pos) * sizeof(SubjectChar)));
    if (char_pos == NULL) return -1;
    char_pos = AlignDown(char_pos, sizeof(SubjectChar));
    pos = static_cast<int>(char_pos - subject.start());
    if (subject[pos] == search_char) return pos;
  } while (++pos < max_n);

  return -1;
}


//---------------------------------------------------------------------
// Single Character Pattern Search Strategy
//---------------------------------------------------------------------

template <typename PatternChar, typename SubjectChar>
int StringSearch<PatternChar, SubjectChar>::SingleCharSearch(
    StringSearch<PatternChar, SubjectChar>* search,
    Vector<const SubjectChar> subject,
    int index) {
  DCHECK_EQ(1, search->pattern_.length());
  PatternChar pattern_first_char = search->pattern_[0];
  if (sizeof(PatternChar) > sizeof(SubjectChar)) {
    if (exceedsOneByte(pattern_first_char)) {
      return -1;
    }
  }
  return FindFirstCharacter(search->pattern_, subject, index);
}

//---------------------------------------------------------------------
// Linear Search Strategy
//---------------------------------------------------------------------


template <typename PatternChar, typename SubjectChar>
inline bool CharCompare(const PatternChar* pattern,
                        const SubjectChar* subject,
                        int length) {
  DCHECK(length > 0);
  int pos = 0;
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
int StringSearch<PatternChar, SubjectChar>::LinearSearch(
    StringSearch<PatternChar, SubjectChar>* search,
    Vector<const SubjectChar> subject,
    int index) {
  Vector<const PatternChar> pattern = search->pattern_;
  DCHECK(pattern.length() > 1);
  int pattern_length = pattern.length();
  int i = index;
  int n = subject.length() - pattern_length;
  while (i <= n) {
    i = FindFirstCharacter(pattern, subject, i);
    if (i == -1) return -1;
    DCHECK_LE(i, n);
    i++;
    // Loop extracted to separate function to allow using return to do
    // a deeper break.
    if (CharCompare(pattern.start() + 1,
                    subject.start() + i,
                    pattern_length - 1)) {
      return i - 1;
    }
  }
  return -1;
}

//---------------------------------------------------------------------
// Boyer-Moore string search
//---------------------------------------------------------------------

template <typename PatternChar, typename SubjectChar>
int StringSearch<PatternChar, SubjectChar>::BoyerMooreSearch(
    StringSearch<PatternChar, SubjectChar>* search,
    Vector<const SubjectChar> subject,
    int start_index) {
  Vector<const PatternChar> pattern = search->pattern_;
  int subject_length = subject.length();
  int pattern_length = pattern.length();
  // Only preprocess at most kBMMaxShift last characters of pattern.
  int start = search->start_;

  int* bad_char_occurence = search->bad_char_table();
  int* good_suffix_shift = search->good_suffix_shift_table();

  PatternChar last_char = pattern[pattern_length - 1];
  int index = start_index;
  // Continue search from i.
  while (index <= subject_length - pattern_length) {
    int j = pattern_length - 1;
    int c;
    while (last_char != (c = subject[index + j])) {
      int shift =
          j - CharOccurrence(bad_char_occurence, c);
      index += shift;
      if (index > subject_length - pattern_length) {
        return -1;
      }
    }
    while (j >= 0 && pattern[j] == (c = subject[index + j])) j--;
    if (j < 0) {
      return index;
    } else if (j < start) {
      // we have matched more than our tables allow us to be smart about.
      // Fall back on BMH shift.
      index += pattern_length - 1
          - CharOccurrence(bad_char_occurence,
                           static_cast<SubjectChar>(last_char));
    } else {
      int gs_shift = good_suffix_shift[j + 1];
      int bc_occ =
          CharOccurrence(bad_char_occurence, c);
      int shift = j - bc_occ;
      if (gs_shift > shift) {
        shift = gs_shift;
      }
      index += shift;
    }
  }

  return -1;
}


template <typename PatternChar, typename SubjectChar>
void StringSearch<PatternChar, SubjectChar>::PopulateBoyerMooreTable() {
  int pattern_length = pattern_.length();
  const PatternChar* pattern = pattern_.start();
  // Only look at the last kBMMaxShift characters of pattern (from start_
  // to pattern_length).
  int start = start_;
  int length = pattern_length - start;

  // Biased tables so that we can use pattern indices as table indices,
  // even if we only cover the part of the pattern from offset start.
  int* shift_table = good_suffix_shift_table();
  int* suffix_table = this->suffix_table();

  // Initialize table.
  for (int i = start; i < pattern_length; i++) {
    shift_table[i] = length;
  }
  shift_table[pattern_length] = 1;
  suffix_table[pattern_length] = pattern_length + 1;

  if (pattern_length <= start) {
    return;
  }

  // Find suffixes.
  PatternChar last_char = pattern[pattern_length - 1];
  int suffix = pattern_length + 1;
  {
    int i = pattern_length;
    while (i > start) {
      PatternChar c = pattern[i - 1];
      while (suffix <= pattern_length && c != pattern[suffix - 1]) {
        if (shift_table[suffix] == length) {
          shift_table[suffix] = suffix - i;
        }
        suffix = suffix_table[suffix];
      }
      suffix_table[--i] = --suffix;
      if (suffix == pattern_length) {
        // No suffix to extend, so we check against last_char only.
        while ((i > start) && (pattern[i - 1] != last_char)) {
          if (shift_table[pattern_length] == length) {
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
    for (int i = start; i <= pattern_length; i++) {
      if (shift_table[i] == length) {
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
int StringSearch<PatternChar, SubjectChar>::BoyerMooreHorspoolSearch(
    StringSearch<PatternChar, SubjectChar>* search,
    Vector<const SubjectChar> subject,
    int start_index) {
  Vector<const PatternChar> pattern = search->pattern_;
  int subject_length = subject.length();
  int pattern_length = pattern.length();
  int* char_occurrences = search->bad_char_table();
  int badness = -pattern_length;

  // How bad we are doing without a good-suffix table.
  PatternChar last_char = pattern[pattern_length - 1];
  int last_char_shift = pattern_length - 1 -
      CharOccurrence(char_occurrences, static_cast<SubjectChar>(last_char));
  // Perform search
  int index = start_index;  // No matches found prior to this index.
  while (index <= subject_length - pattern_length) {
    int j = pattern_length - 1;
    int subject_char;
    while (last_char != (subject_char = subject[index + j])) {
      int bc_occ = CharOccurrence(char_occurrences, subject_char);
      int shift = j - bc_occ;
      index += shift;
      badness += 1 - shift;  // at most zero, so badness cannot increase.
      if (index > subject_length - pattern_length) {
        return -1;
      }
    }
    j--;
    while (j >= 0 && pattern[j] == (subject[index + j])) j--;
    if (j < 0) {
      return index;
    } else {
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
  }
  return -1;
}


template <typename PatternChar, typename SubjectChar>
void StringSearch<PatternChar, SubjectChar>::PopulateBoyerMooreHorspoolTable() {
  int pattern_length = pattern_.length();

  int* bad_char_occurrence = bad_char_table();

  // Only preprocess at most kBMMaxShift last characters of pattern.
  int start = start_;
  // Run forwards to populate bad_char_table, so that *last* instance
  // of character equivalence class is the one registered.
  // Notice: Doesn't include the last character.
  int table_size = AlphabetSize();
  if (start == 0) {  // All patterns less than kBMMaxShift in length.
    memset(bad_char_occurrence,
           -1,
           table_size * sizeof(*bad_char_occurrence));
  } else {
    for (int i = 0; i < table_size; i++) {
      bad_char_occurrence[i] = start - 1;
    }
  }
  for (int i = start; i < pattern_length - 1; i++) {
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
int StringSearch<PatternChar, SubjectChar>::InitialSearch(
    StringSearch<PatternChar, SubjectChar>* search,
    Vector<const SubjectChar> subject,
    int index) {
  Vector<const PatternChar> pattern = search->pattern_;
  int pattern_length = pattern.length();
  // Badness is a count of how much work we have done.  When we have
  // done enough work we decide it's probably worth switching to a better
  // algorithm.
  int badness = -10 - (pattern_length << 2);

  // We know our pattern is at least 2 characters, we cache the first so
  // the common case of the first character not matching is faster.
  for (int i = index, n = subject.length() - pattern_length; i <= n; i++) {
    badness++;
    if (badness <= 0) {
      i = FindFirstCharacter(pattern, subject, i);
      if (i == -1) return -1;
      DCHECK_LE(i, n);
      int j = 1;
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
  return -1;
}


// Perform a a single stand-alone search.
// If searching multiple times for the same pattern, a search
// object should be constructed once and the Search function then called
// for each search.
template <typename SubjectChar, typename PatternChar>
int SearchString(Isolate* isolate,
                 Vector<const SubjectChar> subject,
                 Vector<const PatternChar> pattern,
                 int start_index) {
  StringSearch<PatternChar, SubjectChar> search(isolate, pattern);
  return search.Search(subject, start_index);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_STRING_SEARCH_H_
