// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_SPECIAL_CASE_H_
#define V8_REGEXP_SPECIAL_CASE_H_

#ifdef V8_INTL_SUPPORT
#include "src/base/logging.h"
#include "src/common/globals.h"

#include "unicode/uchar.h"
#include "unicode/uniset.h"

namespace v8 {
namespace internal {
namespace regexp {

// Case equivalence for ignoreCase matching.

// For non-unicode ignoreCase matches (aka "i", not "iu"), ECMA 262
// defines slightly different case-folding rules than Unicode. An
// input character should match a pattern character if the result of
// the Canonicalize algorithm is the same for both characters.
//
// Roughly speaking, for "i" regexps, Canonicalize(c) is the same as
// c.toUpperCase(), unless a) c.toUpperCase() is a multi-character
// string, or b) c is non-ASCII, and c.toUpperCase() is ASCII. See
// https://tc39.es/ecma262/#sec-runtime-semantics-canonicalize-ch for
// the precise definition.
//
// While compiling such regular expressions, we need to compute the
// set of characters that should match a given input character. (See
// GetCaseIndependentLetters and CharacterRange::AddCaseEquivalents.)
// For almost all characters, this can be efficiently computed using
// UnicodeSet::closeOver(USET_SIMPLE_CASE_INSENSITIVE). IgnoreSet represents
// the remaining exceptions: characters that must match only themselves.
//
// If c is in IgnoreSet, it should match only itself. For example,
// U+00DF LATIN SMALL LETTER SHARP S uppercases to "SS", so it
// canonicalizes to itself and must not match U+1E9E LATIN CAPITAL
// LETTER SHARP S, even though Unicode case closure connects them.
//
// Otherwise, closeOver produces the correct characters after removing
// IgnoreSet. For example, closing over 'k' adds U+212A KELVIN SIGN,
// which is in IgnoreSet and must not match 'k' in non-unicode mode.
//
// The contents of IgnoreSet are calculated at build time by
// src/regexp/gen-regexp-special-case.cc, which generates
// gen/src/regexp/special-case.cc. This is done by iterating over the
// result of closeOver for each BMP character, and finding sets for
// which at least one character has a different canonical value than
// another character. Characters that match no other characters in
// their equivalence class are added to IgnoreSet. The generator verifies
// that each Unicode class has at most one non-trivial JS class.

class V8_EXPORT_PRIVATE CaseFolding final : public AllStatic {
 public:
  enum class Mode { kNonUnicode, kUnicode };

  // Equal keys denote matching characters, but can diverge from the
  // specification's Canonicalize operation. Non-unicode inputs are UTF-16 code
  // units; Unicode inputs are code points.
  static UChar32 EquivalenceKey(UChar32 c, Mode mode) {
    DCHECK_GE(c, 0);
    DCHECK_LE(c, mode == Mode::kNonUnicode ? 0xffff : 0x10ffff);
    if (mode == Mode::kNonUnicode && IgnoreSet().contains(c)) return c;
    return u_foldCase(c, U_FOLD_CASE_DEFAULT);
  }

  // Close a set of characters under case equivalence, preserving its
  // original members. Non-unicode inputs must be UTF-16 code units.
  static void CloseOver(icu::UnicodeSet& set, Mode mode) {
    if (mode == Mode::kUnicode) {
      set.closeOver(USET_SIMPLE_CASE_INSENSITIVE);
      return;
    }
    if (IgnoreSet().containsNone(set)) {
      set.closeOver(USET_SIMPLE_CASE_INSENSITIVE);
      set.removeAll(IgnoreSet());
      return;
    }
    if (IgnoreSet().containsAll(set)) return;
    // Ignored characters neither contribute nor acquire equivalents, but
    // must remain in the result if they were explicitly present.
    icu::UnicodeSet ignored(set);
    ignored.retainAll(IgnoreSet());
    set.removeAll(IgnoreSet());
    set.closeOver(USET_SIMPLE_CASE_INSENSITIVE);
    set.removeAll(IgnoreSet());
    set.addAll(ignored);
  }

 private:
  static const icu::UnicodeSet& IgnoreSet();
};

}  // namespace regexp
}  // namespace internal
}  // namespace v8

#endif  // V8_INTL_SUPPORT

#endif  // V8_REGEXP_SPECIAL_CASE_H_
