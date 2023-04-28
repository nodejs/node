// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "src/base/strings.h"
#include "src/regexp/special-case.h"
#include "unicode/usetiter.h"

namespace v8 {
namespace internal {

static const base::uc32 kSurrogateStart = 0xd800;
static const base::uc32 kSurrogateEnd = 0xdfff;
static const base::uc32 kNonBmpStart = 0x10000;

// The following code generates "src/regexp/special-case.cc".
void PrintSet(std::ofstream& out, const char* name,
              const icu::UnicodeSet& set) {
  out << "icu::UnicodeSet Build" << name << "() {\n"
      << "  icu::UnicodeSet set;\n";
  for (int32_t i = 0; i < set.getRangeCount(); i++) {
    if (set.getRangeStart(i) == set.getRangeEnd(i)) {
      out << "  set.add(0x" << set.getRangeStart(i) << ");\n";
    } else {
      out << "  set.add(0x" << set.getRangeStart(i) << ", 0x"
          << set.getRangeEnd(i) << ");\n";
    }
  }
  out << "  set.freeze();\n"
      << "  return set;\n"
      << "}\n\n";

  out << "struct " << name << "Data {\n"
      << "  " << name << "Data() : set(Build" << name << "()) {}\n"
      << "  const icu::UnicodeSet set;\n"
      << "};\n\n";

  out << "//static\n"
      << "const icu::UnicodeSet& RegExpCaseFolding::" << name << "() {\n"
      << "  static base::LazyInstance<" << name << "Data>::type set =\n"
      << "      LAZY_INSTANCE_INITIALIZER;\n"
      << "  return set.Pointer()->set;\n"
      << "}\n\n";
}

void PrintSpecial(std::ofstream& out) {
  icu::UnicodeSet current;
  icu::UnicodeSet special_add;
  icu::UnicodeSet ignore;
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeSet upper("[\\p{Lu}]", status);
  CHECK(U_SUCCESS(status));

  // Iterate through all chars in BMP except surrogates.
  for (UChar32 i = 0; i < static_cast<UChar32>(kNonBmpStart); i++) {
    if (i >= static_cast<UChar32>(kSurrogateStart) &&
        i <= static_cast<UChar32>(kSurrogateEnd)) {
      continue;  // Ignore surrogate range
    }
    current.set(i, i);
    current.closeOver(USET_CASE_INSENSITIVE);

    // Check to see if all characters in the case-folding equivalence
    // class as defined by UnicodeSet::closeOver all map to the same
    // canonical value.
    UChar32 canonical = RegExpCaseFolding::Canonicalize(i);
    bool class_has_matching_canonical_char = false;
    bool class_has_non_matching_canonical_char = false;
    for (int32_t j = 0; j < current.getRangeCount(); j++) {
      for (UChar32 c = current.getRangeStart(j); c <= current.getRangeEnd(j);
           c++) {
        if (c == i) {
          continue;
        }
        UChar32 other_canonical = RegExpCaseFolding::Canonicalize(c);
        if (canonical == other_canonical) {
          class_has_matching_canonical_char = true;
        } else {
          class_has_non_matching_canonical_char = true;
        }
      }
    }
    // If any other character in i's equivalence class has a
    // different canonical value, then i needs special handling.  If
    // no other character shares a canonical value with i, we can
    // ignore i when adding alternatives for case-independent
    // comparison.  If at least one other character shares a
    // canonical value, then i needs special handling.
    if (class_has_non_matching_canonical_char) {
      if (class_has_matching_canonical_char) {
        special_add.add(i);
      } else {
        ignore.add(i);
      }
    }
  }

  // Verify that no Unicode equivalence class contains two non-trivial
  // JS equivalence classes. Every character in SpecialAddSet has the
  // same canonical value as every other non-IgnoreSet character in
  // its Unicode equivalence class. Therefore, if we call closeOver on
  // a set containing no IgnoreSet characters, the only characters
  // that must be removed from the result are in IgnoreSet. This fact
  // is used in CharacterRange::AddCaseEquivalents.
  for (int32_t i = 0; i < special_add.getRangeCount(); i++) {
    for (UChar32 c = special_add.getRangeStart(i);
         c <= special_add.getRangeEnd(i); c++) {
      UChar32 canonical = RegExpCaseFolding::Canonicalize(c);
      current.set(c, c);
      current.closeOver(USET_CASE_INSENSITIVE);
      current.removeAll(ignore);
      for (int32_t j = 0; j < current.getRangeCount(); j++) {
        for (UChar32 c2 = current.getRangeStart(j);
             c2 <= current.getRangeEnd(j); c2++) {
          CHECK_EQ(canonical, RegExpCaseFolding::Canonicalize(c2));
        }
      }
    }
  }

  PrintSet(out, "IgnoreSet", ignore);
  PrintSet(out, "SpecialAddSet", special_add);
}

void PrintUnicodeSpecial(std::ofstream& out) {
  icu::UnicodeSet non_simple_folding;
  icu::UnicodeSet current;
  UErrorCode status = U_ZERO_ERROR;
  // Look at all characters except white spaces.
  icu::UnicodeSet interestingCP(u"[^[:White_Space:]]", status);
  CHECK_EQ(status, U_ZERO_ERROR);
  icu::UnicodeSetIterator iter(interestingCP);
  while (iter.next()) {
    UChar32 c = iter.getCodepoint();
    current.set(c, c);
    current.closeOver(USET_CASE_INSENSITIVE).removeAllStrings();
    CHECK(!current.isBogus());
    // Remove characters from the closeover that have a simple case folding.
    icu::UnicodeSet toRemove;
    icu::UnicodeSetIterator closeOverIter(current);
    while (closeOverIter.next()) {
      UChar32 closeOverChar = closeOverIter.getCodepoint();
      UChar32 closeOverSCF = u_foldCase(closeOverChar, U_FOLD_CASE_DEFAULT);
      if (closeOverChar != closeOverSCF) {
        toRemove.add(closeOverChar);
      }
    }
    CHECK(!toRemove.isBogus());
    current.removeAll(toRemove);

    // The current character and its simple case folding are also always OK.
    UChar32 scf = u_foldCase(c, U_FOLD_CASE_DEFAULT);
    current.remove(c);
    current.remove(scf);

    // If there are any characters remaining, they were added due to full case
    // foldings and shouldn't match the current charcter according to the spec.
    if (!current.isEmpty()) {
      // Ensure that the character doesn't have a simple case folding.
      // Otherwise the current approach of simply removing the character from
      // the set before calling closeOver won't work.
      CHECK_EQ(c, scf);
      non_simple_folding.add(c);
    }
  }
  CHECK(!non_simple_folding.isBogus());

  PrintSet(out, "UnicodeNonSimpleCloseOverSet", non_simple_folding);
}

void WriteHeader(const char* header_filename) {
  std::ofstream out(header_filename);
  out << std::hex << std::setfill('0') << std::setw(4);
  out << "// Copyright 2020 the V8 project authors. All rights reserved.\n"
      << "// Use of this source code is governed by a BSD-style license that\n"
      << "// can be found in the LICENSE file.\n\n"
      << "// Automatically generated by regexp/gen-regexp-special-case.cc\n\n"
      << "// The following functions are used to build UnicodeSets\n"
      << "// for special cases where the case-folding algorithm used by\n"
      << "// UnicodeSet::closeOver(USET_CASE_INSENSITIVE) does not match\n"
      << "// the algorithm defined in ECMAScript 2020 21.2.2.8.2 (Runtime\n"
      << "// Semantics: Canonicalize) step 3.\n\n"
      << "#ifdef V8_INTL_SUPPORT\n"
      << "#include \"src/base/lazy-instance.h\"\n\n"
      << "#include \"src/regexp/special-case.h\"\n\n"
      << "#include \"unicode/uniset.h\"\n"
      << "namespace v8 {\n"
      << "namespace internal {\n\n";

  PrintSpecial(out);
  PrintUnicodeSpecial(out);

  out << "\n"
      << "}  // namespace internal\n"
      << "}  // namespace v8\n"
      << "#endif  // V8_INTL_SUPPORT\n";
}

}  // namespace internal
}  // namespace v8

int main(int argc, const char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <output filename>\n";
    std::exit(1);
  }
  v8::internal::WriteHeader(argv[1]);

  return 0;
}
