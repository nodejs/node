// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows("/\p{Block=ASCII}+/u");
assertThrows("/\p{Block=ASCII}+/u");
assertThrows("/\p{Block=Basic_Latin}+/u");
assertThrows("/\p{Block=Basic_Latin}+/u");

assertThrows("/\p{blk=CJK}+/u");
assertThrows("/\p{blk=CJK_Unified_Ideographs}+/u");
assertThrows("/\p{blk=CJK}+/u");
assertThrows("/\p{blk=CJK_Unified_Ideographs}+/u");

assertThrows("/\p{Block=ASCII}+/u");
assertThrows("/\p{Block=ASCII}+/u");
assertThrows("/\p{Block=Basic_Latin}+/u");
assertThrows("/\p{Block=Basic_Latin}+/u");

assertThrows("/\p{NFKD_Quick_Check=Y}+/u");
assertThrows("/\p{NFKD_QC=Yes}+/u");

assertThrows("/\p{Numeric_Type=Decimal}+/u");
assertThrows("/\p{nt=De}+/u");

assertThrows("/\p{Bidi_Class=Arabic_Letter}+/u");
assertThrows("/\p{Bidi_Class=AN}+/u");

assertThrows("/\p{ccc=OV}+/u");

assertThrows("/\p{Sentence_Break=Format}+/u");

assertThrows("/\\p{In}/u");
assertThrows("/\\pI/u");
assertThrows("/\\p{I}/u");
assertThrows("/\\p{CJK}/u");

assertThrows("/\\p{}/u");
