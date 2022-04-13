// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testRegExpI(text, msg) {
  assertTrue(new RegExp(text, 'i').test(text.toUpperCase()), msg + ': ' + text);
}

testRegExpI('abc', 'ASCII');
testRegExpI('ABC', 'ASCII');
testRegExpI('rst', 'ASCII');
testRegExpI('RST', 'ASCII');

testRegExpI('αβψδεφ', 'Greek');

testRegExpI('\u1c80\u1c81', 'Historic Cyrillic added in Unicode 9');
testRegExpI('\u026A', 'Dotless I, uppercase form added in Unicode 9');

testRegExpI('ოქტ', 'Georgian Mtavruli added in Unicode 11');
