// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function t(re, s) { assertTrue(re.test(s)); }
function f(re, s) { assertFalse(re.test(s)); }

assertThrows("/\\p{Hiragana}/u");
assertThrows("/\\p{Bidi_Class}/u");
assertThrows("/\\p{Bidi_C=False}/u");
assertThrows("/\\P{Bidi_Control=Y}/u");
assertThrows("/\\p{AHex=Yes}/u");

assertThrows("/\\p{Composition_Exclusion}/u");
assertThrows("/\\p{CE}/u");
assertThrows("/\\p{Full_Composition_Exclusion}/u");
assertThrows("/\\p{Comp_Ex}/u");
assertThrows("/\\p{Grapheme_Link}/u");
assertThrows("/\\p{Gr_Link}/u");
assertThrows("/\\p{Hyphen}/u");
assertThrows("/\\p{NFD_Inert}/u");
assertThrows("/\\p{NFDK_Inert}/u");
assertThrows("/\\p{NFC_Inert}/u");
assertThrows("/\\p{NFKC_Inert}/u");
assertThrows("/\\p{Segment_Starter}/u");

t(/\p{Alphabetic}/u, "æ");
f(/\p{Alpha}/u, "1");

t(/\p{ASCII_Hex_Digit}/u, "f");
f(/\p{AHex}/u, "g");

t(/\p{Bidi_Control}/u, "\u200e");
f(/\p{Bidi_C}/u, "g");

t(/\p{Bidi_Mirrored}/u, "(");
f(/\p{Bidi_M}/u, "-");

t(/\p{Case_Ignorable}/u, "\u02b0");
f(/\p{CI}/u, "a");

t(/\p{Changes_When_Casefolded}/u, "B");
f(/\p{CWCF}/u, "1");

t(/\p{Changes_When_Casemapped}/u, "b");
f(/\p{CWCM}/u, "1");

t(/\p{Changes_When_Lowercased}/u, "B");
f(/\p{CWL}/u, "1");

t(/\p{Changes_When_Titlecased}/u, "b");
f(/\p{CWT}/u, "1");

t(/\p{Changes_When_Uppercased}/u, "b");
f(/\p{CWU}/u, "1");

t(/\p{Dash}/u, "-");
f(/\p{Dash}/u, "1");

t(/\p{Default_Ignorable_Code_Point}/u, "\u00ad");
f(/\p{DI}/u, "1");

t(/\p{Deprecated}/u, "\u17a3");
f(/\p{Dep}/u, "1");

t(/\p{Diacritic}/u, "\u0301");
f(/\p{Dia}/u, "1");

t(/\p{Emoji}/u, "\u2603");
f(/\p{Emoji}/u, "x");

t(/\p{Emoji_Component}/u, "\u{1F1E6}");
f(/\p{Emoji_Component}/u, "x");

t(/\p{Emoji_Modifier_Base}/u, "\u{1F6CC}");
f(/\p{Emoji_Modifier_Base}/u, "x");

t(/\p{Emoji_Modifier}/u, "\u{1F3FE}");
f(/\p{Emoji_Modifier}/u, "x");

t(/\p{Emoji_Presentation}/u, "\u{1F308}");
f(/\p{Emoji_Presentation}/u, "x");

t(/\p{Extender}/u, "\u3005");
f(/\p{Ext}/u, "x");

t(/\p{Grapheme_Base}/u, " ");
f(/\p{Gr_Base}/u, "\u0010");

t(/\p{Grapheme_Extend}/u, "\u0300");
f(/\p{Gr_Ext}/u, "x");

t(/\p{Hex_Digit}/u, "a");
f(/\p{Hex}/u, "g");

t(/\p{ID_Continue}/u, "1");
f(/\p{IDC}/u, ".");

t(/\p{ID_Start}/u, "a");
f(/\p{IDS}/u, "1");

t(/\p{Ideographic}/u, "漢");
f(/\p{Ideo}/u, "H");

t(/\p{IDS_Binary_Operator}/u, "\u2FF0");
f(/\p{IDSB}/u, "a");

t(/\p{IDS_Trinary_Operator}/u, "\u2FF2");
f(/\p{IDST}/u, "a");

t(/\p{Join_Control}/u, "\u200c");
f(/\p{Join_C}/u, "a");

t(/\p{Logical_Order_Exception}/u, "\u0e40");
f(/\p{LOE}/u, "a");

t(/\p{Lowercase}/u, "a");
f(/\p{Lower}/u, "A");

t(/\p{Math}/u, "=");
f(/\p{Math}/u, "A");

t(/\p{Noncharacter_Code_Point}/u, "\uFDD0");
f(/\p{NChar}/u, "A");

t(/\p{Pattern_Syntax}/u, "\u0021");
f(/\p{NChar}/u, "A");

t(/\p{Pattern_White_Space}/u, "\u0009");
f(/\p{Pat_Syn}/u, "A");

t(/\p{Quotation_Mark}/u, "'");
f(/\p{QMark}/u, "A");

t(/\p{Radical}/u, "\u2FAD");
f(/\p{Radical}/u, "A");

t(/\p{Regional_Indicator}/u, "\u{1F1E6}");
f(/\p{Regional_Indicator}/u, "A");

t(/\p{Sentence_Terminal}/u, "!");
f(/\p{STerm}/u, "A");

t(/\p{Soft_Dotted}/u, "i");
f(/\p{SD}/u, "A");

t(/\p{Terminal_Punctuation}/u, ".");
f(/\p{Term}/u, "A");

t(/\p{Unified_Ideograph}/u, "\u4e00");
f(/\p{UIdeo}/u, "A");

t(/\p{Uppercase}/u, "A");
f(/\p{Upper}/u, "a");

t(/\p{Variation_Selector}/u, "\uFE00");
f(/\p{VS}/u, "A");

t(/\p{White_Space}/u, " ");
f(/\p{WSpace}/u, "A");

t(/\p{XID_Continue}/u, "1");
f(/\p{XIDC}/u, " ");

t(/\p{XID_Start}/u, "A");
f(/\p{XIDS}/u, " ");
