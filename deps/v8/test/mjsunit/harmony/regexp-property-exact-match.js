// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-property --harmony-unicode-regexps

assertThrows("/\\p{In CJK}/u");
assertThrows("/\\p{InCJKUnifiedIdeographs}/u");
assertDoesNotThrow("/\\p{InCJK}/u");
assertDoesNotThrow("/\\p{InCJK_Unified_Ideographs}/u");

assertDoesNotThrow("/\\p{InCyrillic_Sup}/u");
assertDoesNotThrow("/\\p{InCyrillic_Supplement}/u");
assertDoesNotThrow("/\\p{InCyrillic_Supplementary}/u");
assertThrows("/\\p{InCyrillicSupplementary}/u");
assertThrows("/\\p{InCyrillic_supplementary}/u");

assertDoesNotThrow("/\\pC/u");
assertDoesNotThrow("/\\p{Other}/u");
assertDoesNotThrow("/\\p{Cc}/u");
assertDoesNotThrow("/\\p{Control}/u");
assertDoesNotThrow("/\\p{cntrl}/u");
assertDoesNotThrow("/\\p{M}/u");
assertDoesNotThrow("/\\p{Mark}/u");
assertDoesNotThrow("/\\p{Combining_Mark}/u");
assertThrows("/\\p{Combining Mark}/u");

assertDoesNotThrow("/\\p{Copt}/u");
assertDoesNotThrow("/\\p{Coptic}/u");
assertDoesNotThrow("/\\p{Qaac}/u");
assertDoesNotThrow("/\\p{Egyp}/u");
assertDoesNotThrow("/\\p{Egyptian_Hieroglyphs}/u");
assertThrows("/\\p{EgyptianHieroglyphs}/u");
