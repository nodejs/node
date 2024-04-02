// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

assertEquals(String.fromCharCode(97, 220, 256), 'a' + '\u00DC' + '\u0100');
assertEquals(String.fromCharCode(97, 220, 256), 'a\u00DC\u0100');

assertEquals(0x80, JSON.stringify("\x80").charCodeAt(1));
assertEquals(0x80, JSON.stringify("\x80", 0, null).charCodeAt(1));

assertEquals(['a', 'b', '\xdc'], ['b', '\xdc', 'a'].sort());

assertEquals(['\xfc\xdc', '\xfc'], new RegExp('(\xdc)\\1', 'i').exec('\xfc\xdc'));
// Same test but for all values in Latin-1 range.
var total_lo = 0;
for (var i = 0; i < 0xff; i++) {
  var base = String.fromCharCode(i);
  var escaped = base;
  if (base == '(' || base == ')' || base == '*' || base == '+' ||
      base == '?' || base == '[' || base == ']' || base == '\\' ||
      base == '$' || base == '^' || base == '|') {
    escaped = '\\' + base;
  }
  var lo = String.fromCharCode(i + 0x20);
  base_result = new RegExp('(' + escaped + ')\\1', 'i').exec(base + base);
  assertEquals( base_result, [base + base, base]);
  lo_result = new RegExp('(' + escaped + ')\\1', 'i').exec(base + lo);
  if (base.toLowerCase() == lo) {
    assertEquals([base + lo, base], lo_result);
    total_lo++;
  } else {
    assertEquals(null, lo_result);
  }
}
// Should have hit the branch for the following char codes:
// [A-Z], [192-222] but not 215
assertEquals((90-65+1)+(222-192-1+1), total_lo);

// Latin-1 whitespace character
assertEquals( 1, +(String.fromCharCode(0xA0) + '1') );

// Latin-1 \W characters
assertEquals(["+\u00a3", "=="], "+\u00a3==".match(/\W\W/g));

// Latin-1 character that uppercases out of Latin-1.
assertTrue(/\u0178/i.test('\u00ff'));

// Unicode equivalence
assertTrue(/\u039c/i.test('\u00b5'));
assertTrue(/\u039c/i.test('\u03bc'));
assertTrue(/\u00b5/i.test('\u03bc'));
// Unicode equivalence ranges
assertTrue(/[\u039b-\u039d]/i.test('\u00b5'));
assertFalse(/[^\u039b-\u039d]/i.test('\u00b5'));
assertFalse(/[\u039b-\u039d]/.test('\u00b5'));
assertTrue(/[^\u039b-\u039d]/.test('\u00b5'));

// Check a regression in QuoteJsonSlow and WriteQuoteJsonString
for (var testNumber = 0; testNumber < 2; testNumber++) {
  var testString = "\xdc";
  var loopLength = testNumber == 0 ? 0 : 20;
  for (var i = 0; i < loopLength; i++ ) {
    testString += testString;
  }
  var stringified = JSON.stringify({"test" : testString}, null, 0);
  var stringifiedExpected = '{"test":"' + testString + '"}';
  assertEquals(stringifiedExpected, stringified);
}
