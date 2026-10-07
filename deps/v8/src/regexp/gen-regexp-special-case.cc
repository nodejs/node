// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "src/base/strings.h"
#include "src/regexp/special-case.h"
#include "unicode/locid.h"
#include "unicode/unistr.h"

namespace v8 {
namespace internal {
namespace regexp {

static const base::uc32 kSurrogateStart = 0xd800;
static const base::uc32 kSurrogateEnd = 0xdfff;
static const base::uc32 kNonBmpStart = 0x10000;

// This implements ECMAScript 2020 21.2.2.8.2 (Runtime Semantics:
// Canonicalize) step 3, which is used to determine whether
// characters match when ignoreCase is true and unicode is false.
UChar32 Canonicalize(UChar32 ch) {
  // a. Assert: ch is a UTF-16 code unit.
  CHECK_LE(ch, 0xffff);

  // b. Let s be the String value consisting of the single code unit ch.
  icu::UnicodeString s(ch);

  // c. Let u be the same result produced as if by performing the algorithm
  // for String.prototype.toUpperCase using s as the this value.
  // d. Assert: Type(u) is String.
  icu::UnicodeString& u = s.toUpper(icu::Locale::getRoot());

  // e. If u does not consist of a single code unit, return ch.
  if (u.length() != 1) {
    return ch;
  }

  // f. Let cu be u's single code unit element.
  UChar32 cu = u.char32At(0);

  // g. If the value of ch >= 128 and the value of cu < 128, return ch.
  if (ch >= 128 && cu < 128) {
    return ch;
  }

  // h. Return cu.
  return cu;
}

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
      << "const icu::UnicodeSet& CaseFolding::" << name << "() {\n"
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
    current.closeOver(USET_SIMPLE_CASE_INSENSITIVE);

    // Check to see if all characters in the case-folding equivalence
    // class as defined by UnicodeSet::closeOver all map to the same
    // canonical value.
    UChar32 canonical = Canonicalize(i);
    bool class_has_matching_canonical_char = false;
    bool class_has_non_matching_canonical_char = false;
    for (int32_t j = 0; j < current.getRangeCount(); j++) {
      for (UChar32 c = current.getRangeStart(j); c <= current.getRangeEnd(j);
           c++) {
        if (c == i) {
          continue;
        }
        UChar32 other_canonical = Canonicalize(c);
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
  // JS equivalence classes. Every character in special_add has the
  // same canonical value as every other non-IgnoreSet character in
  // its Unicode equivalence class. Therefore, if we call closeOver on
  // a set containing no IgnoreSet characters, the only characters
  // that must be removed from the result are in IgnoreSet. This fact
  // is used in CaseFolding::CloseOver.
  for (int32_t i = 0; i < special_add.getRangeCount(); i++) {
    for (UChar32 c = special_add.getRangeStart(i);
         c <= special_add.getRangeEnd(i); c++) {
      UChar32 canonical = Canonicalize(c);
      current.set(c, c);
      current.closeOver(USET_SIMPLE_CASE_INSENSITIVE);
      current.removeAll(ignore);
      for (int32_t j = 0; j < current.getRangeCount(); j++) {
        for (UChar32 c2 = current.getRangeStart(j);
             c2 <= current.getRangeEnd(j); c2++) {
          CHECK_EQ(canonical, Canonicalize(c2));
        }
      }
    }
  }

  PrintSet(out, "IgnoreSet", ignore);
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
      << "// UnicodeSet::closeOver(USET_SIMPLE_CASE_INSENSITIVE) does\n"
      << "// not match the algorithm defined in ECMAScript 2020 21.2.2.8.2\n"
      << "// (Runtime Semantics: Canonicalize) step 3.\n\n"
      << "#ifdef V8_INTL_SUPPORT\n"
      << "#include \"src/base/lazy-instance.h\"\n\n"
      << "#include \"src/regexp/special-case.h\"\n\n"
      << "#include \"unicode/uniset.h\"\n"
      << "namespace v8 {\n"
      << "namespace internal {\n"
      << "namespace regexp {\n\n";

  PrintSpecial(out);

  out << "\n"
      << "}  // namespace regexp\n"
      << "}  // namespace internal\n"
      << "}  // namespace v8\n"
      << "#endif  // V8_INTL_SUPPORT\n";
}

}  // namespace regexp
}  // namespace internal
}  // namespace v8

int main(int argc, const char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <output filename>\n";
    std::exit(1);
  }
  v8::internal::regexp::WriteHeader(argv[1]);

  return 0;
}
