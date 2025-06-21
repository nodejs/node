// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertTrue(/\w/iu.test('\u017F'));
assertTrue(/\w/iu.test('\u212A'));
assertFalse(/\W/iu.test('\u017F'));
assertFalse(/\W/iu.test('\u212A'));
assertFalse(/\W/iu.test('s'));
assertFalse(/\W/iu.test('S'));
assertFalse(/\W/iu.test('K'));
assertFalse(/\W/iu.test('k'));

assertTrue(/[\w]/iu.test('\u017F'));
assertTrue(/[\w]/iu.test('\u212A'));
assertFalse(/[\W]/iu.test('\u017F'));
assertFalse(/[\W]/iu.test('\u212A'));
assertFalse(/[\W]/iu.test('s'));
assertFalse(/[\W]/iu.test('S'));
assertFalse(/[\W]/iu.test('K'));
assertFalse(/[\W]/iu.test('k'));

assertTrue(/\b/iu.test('\u017F'));
assertTrue(/\b/iu.test('\u212A'));
assertTrue(/\b/iu.test('s'));
assertTrue(/\b/iu.test('S'));
assertFalse(/\B/iu.test('\u017F'));
assertFalse(/\B/iu.test('\u212A'));
assertFalse(/\B/iu.test('s'));
assertFalse(/\B/iu.test('S'));
assertFalse(/\B/iu.test('K'));
assertFalse(/\B/iu.test('k'));

assertEquals(["abcd", "d"], /a.*?(.)\b/i.exec('abcd\u017F cd'));
assertEquals(["abcd", "d"], /a.*?(.)\b/i.exec('abcd\u212A cd'));
assertEquals(["abcd\u017F", "\u017F"], /a.*?(.)\b/iu.exec('abcd\u017F cd'));
assertEquals(["abcd\u212A", "\u212A"], /a.*?(.)\b/iu.exec('abcd\u212A cd'));

assertEquals(["a\u017F ", " "], /a.*?\B(.)/i.exec('a\u017F '));
assertEquals(["a\u212A ", " "], /a.*?\B(.)/i.exec('a\u212A '));
assertEquals(["a\u017F", "\u017F"], /a.*?\B(.)/iu.exec('a\u017F '));
assertEquals(["a\u212A", "\u212A"], /a.*?\B(.)/iu.exec('a\u212A '));
