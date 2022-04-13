// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on https://www.ecma-international.org/ecma-402/#table-numbering-system-digits
let testCases = [
  ["arab", "in ١٢٣ days"], // U+0660 to U+0669
  ["arabext", "in ۱۲۳ days"], // U+06F0 to U+06F9
  ["bali", "in ᭑᭒᭓ days"], // U+1B50 to U+1B59
  ["beng", "in ১২৩ days"], // U+09E6 to U+09EF
  ["deva", "in १२३ days"], // U+0966 to U+096F
  ["fullwide", "in １２３ days"], // U+FF10 to U+FF19
  ["gujr", "in ૧૨૩ days"], // U+0AE6 to U+0AEF
  ["guru", "in ੧੨੩ days"], // U+0A66 to U+0A6F
  // U+3007, U+4E00, U+4E8C, U+4E09, U+56DB, U+4E94, U+516D, U+4E03, U+516B, U+4E5D
  ["hanidec", "in 一二三 days"],
  ["khmr", "in ១២៣ days"], // U+17E0 to U+17E9
  ["knda", "in ೧೨೩ days"], // U+0CE6 to U+0CEF
  ["laoo", "in ໑໒໓ days"], // U+0ED0 to U+0ED9
  ["latn", "in 123 days"], // U+0030 to U+0039
  ["limb", "in ᥇᥈᥉ days"], // U+1946 to U+194F
  ["mlym", "in ൧൨൩ days"], // U+0D66 to U+0D6F
  ["mong", "in ᠑᠒᠓ days"], // U+1810 to U+1819
  ["mymr", "in ၁၂၃ days"], // U+1040 to U+1049
  ["orya", "in ୧୨୩ days"], // U+0B66 to U+0B6F
  ["tamldec", "in ௧௨௩ days"], // U+0BE6 to U+0BEF
  ["telu", "in ౧౨౩ days"], // U+0C66 to U+0C6F
  ["thai", "in ๑๒๓ days"], // U+0E50 to U+0E59
  ["tibt", "in ༡༢༣ days"], // U+0F20 to U+0F29
];

for ([numberingSystem, expected] of testCases) {
  let byLocale = new Intl.RelativeTimeFormat("en-u-nu-" + numberingSystem);
  let byOptions = new Intl.RelativeTimeFormat("en", { numberingSystem });

  // Check the numberingSystem in the resolvedOptions matched.
  assertEquals(numberingSystem,
               byOptions.resolvedOptions().numberingSystem, numberingSystem);
  assertEquals(byLocale.resolvedOptions().numberingSystem,
               byOptions.resolvedOptions().numberingSystem, numberingSystem);

  // Check the formatted result are the same as if creating by using -u-nu- in
  // locale.
  assertEquals(byLocale.format(123, "day"), byOptions.format(123, "day"), numberingSystem);
  assertEquals(expected, byLocale.format(123, "day"), numberingSystem);
}
