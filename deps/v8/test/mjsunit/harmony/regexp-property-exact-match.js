// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-property

assertThrows("/\\p{In CJK}/u");
assertThrows("/\\p{InCJKUnifiedIdeographs}/u");
assertThrows("/\\p{InCJK}/u");
assertThrows("/\\p{InCJK_Unified_Ideographs}/u");

assertThrows("/\\p{InCyrillic_Sup}/u");
assertThrows("/\\p{InCyrillic_Supplement}/u");
assertThrows("/\\p{InCyrillic_Supplementary}/u");
assertThrows("/\\p{InCyrillicSupplementary}/u");
assertThrows("/\\p{InCyrillic_supplementary}/u");

assertDoesNotThrow("/\\p{C}/u");
assertDoesNotThrow("/\\p{Other}/u");
assertDoesNotThrow("/\\p{Cc}/u");
assertDoesNotThrow("/\\p{Control}/u");
assertDoesNotThrow("/\\p{cntrl}/u");
assertDoesNotThrow("/\\p{M}/u");
assertDoesNotThrow("/\\p{Mark}/u");
assertDoesNotThrow("/\\p{Combining_Mark}/u");
assertThrows("/\\p{Combining Mark}/u");

assertDoesNotThrow("/\\p{Script=Copt}/u");
assertThrows("/\\p{Coptic}/u");
assertThrows("/\\p{Qaac}/u");
assertThrows("/\\p{Egyp}/u");
assertDoesNotThrow("/\\p{Script=Egyptian_Hieroglyphs}/u");
assertThrows("/\\p{EgyptianHieroglyphs}/u");

assertThrows("/\\p{BidiClass=LeftToRight}/u");
assertThrows("/\\p{BidiC=LeftToRight}/u");
assertThrows("/\\p{bidi_c=Left_To_Right}/u");

assertDoesNotThrow("/\\p{Block=CJK}/u");
assertThrows("/\\p{Block = CJK}/u");
assertThrows("/\\p{Block=cjk}/u");
assertThrows("/\\p{BLK=CJK}/u");
