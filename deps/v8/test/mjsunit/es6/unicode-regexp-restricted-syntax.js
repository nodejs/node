// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// test262/data/test/language/literals/regexp/u-dec-esc
assertThrows("/\\1/u", SyntaxError);
// test262/language/literals/regexp/u-invalid-char-range-a
assertThrows("/[\\w-a]/u", SyntaxError);
// test262/language/literals/regexp/u-invalid-char-range-b
assertThrows("/[a-\\w]/u", SyntaxError);
// test262/language/literals/regexp/u-invalid-char-esc
assertThrows("/\\c/u", SyntaxError);
assertThrows("/\\c0/u", SyntaxError);
// test262/built-ins/RegExp/unicode_restricted_quantifiable_assertion
assertThrows("/(?=.)*/u", SyntaxError);
// test262/built-ins/RegExp/unicode_restricted_octal_escape
assertThrows("/[\\1]/u", SyntaxError);
assertThrows("/\\00/u", SyntaxError);
assertThrows("/\\09/u", SyntaxError);
// test262/built-ins/RegExp/unicode_restricted_identity_escape_alpha
assertThrows("/[\\c]/u", SyntaxError);
// test262/built-ins/RegExp/unicode_restricted_identity_escape_c
assertThrows("/[\\c0]/u", SyntaxError);
// test262/built-ins/RegExp/unicode_restricted_incomple_quantifier
assertThrows("/a{/u", SyntaxError);
assertThrows("/a{1,/u", SyntaxError);
assertThrows("/{/u", SyntaxError);
assertThrows("/}/u", SyntaxError);
// test262/data/test/built-ins/RegExp/unicode_restricted_brackets
assertThrows("/]/u", SyntaxError);
// test262/built-ins/RegExp/unicode_identity_escape
/\//u;

// escaped \0 is allowed inside a character class.
assertEquals(["\0"], /[\0]/u.exec("\0"));
// unless it is followed by another digit.
assertThrows("/[\\00]/u", SyntaxError);
assertThrows("/[\\01]/u", SyntaxError);
assertThrows("/[\\09]/u", SyntaxError);
assertEquals(["\u{0}1\u{0}a\u{0}"], /[1\0a]+/u.exec("b\u{0}1\u{0}a\u{0}2"));
// escaped \- is allowed inside a character class.
assertEquals(["-"], /[a\-z]/u.exec("12-34"));
