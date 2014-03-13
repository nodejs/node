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

// Tests the new String.prototype.normalize method.


// Common use case when searching for 'not very exact' match.
// These are examples of data one might encounter in real use.
var testRealUseCases = function() {
  // Vietnamese legacy text, old Windows 9x / non-Unicode applications use
  // windows-1258 code page, which is neither precomposed, nor decomposed.
  assertEquals('ti\u00ea\u0301ng Vi\u00ea\u0323t'.normalize('NFKD'),
   'ti\u1ebfng Vi\u1ec7t'.normalize('NFKD')); // all precomposed

  // Various kinds of spaces
  assertEquals('Google\u0020Maps'.normalize('NFKD'), // normal space
    'Google\u00a0Maps'.normalize('NFKD')); // non-breaking space
  assertEquals('Google\u0020Maps'.normalize('NFKD'), // normal space
    'Google\u2002Maps'.normalize('NFKD')); // en-space
  assertEquals('Google\u0020Maps'.normalize('NFKD'), // normal space
    'Google\u2003Maps'.normalize('NFKD')); // em-space
  assertEquals('Google\u0020Maps'.normalize('NFKD'), // normal space
    'Google\u3000Maps'.normalize('NFKC')); // ideographic space

  // Latin small ligature "fi"
  assertEquals('fi'.normalize('NFKD'), '\ufb01'.normalize('NFKD'));

  // ŀ, Latin small L with middle dot, used in Catalan and often represented
  // as decomposed for non-Unicode environments ( l + ·)
  assertEquals('l\u00b7'.normalize('NFKD'), '\u0140'.normalize('NFKD'));

  // Legacy text, Japanese narrow Kana (MS-DOS & Win 3.x time)
  assertEquals('\u30d1\u30bd\u30b3\u30f3'.normalize('NFKD'), // パソコン  :  wide
    '\uff8a\uff9f\uff7f\uff7a\uff9d'.normalize('NFKD')); // ﾊﾟｿｺﾝ  :  narrow
  // Also for Japanese, Latin fullwidth forms vs. ASCII
  assertEquals('ABCD'.normalize('NFKD'),
    '\uff21\uff22\uff23\uff24'.normalize('NFKD')); // ＡＢＣＤ, fullwidth
}();


var testEdgeCases = function() {
  // Make sure we throw RangeError, as the standard requires.
  assertThrows('"".normalize(1234)', RangeError);
  assertThrows('"".normalize("BAD")', RangeError);

  // The standard does not say what kind of exceptions we should throw, so we
  // will not be specific. But we still test that we throw errors.
  assertThrows('s.normalize()'); // s is not defined
  assertThrows('var s = null; s.normalize()');
  assertThrows('var s = undefined; s.normalize()');
  assertThrows('var s = 1234; s.normalize()'); // no normalize for non-strings
}();


// Several kinds of mappings. No need to be comprehensive, we don't test
// the ICU functionality, we only test C - JavaScript 'glue'
var testData = [
  // org, default, NFC, NFD, NKFC, NKFD
  ['\u00c7', // Ç : Combining sequence, Latin 1
    '\u00c7', '\u0043\u0327',
    '\u00c7', '\u0043\u0327'],
  ['\u0218', // Ș : Combining sequence, non-Latin 1
    '\u0218', '\u0053\u0326',
    '\u0218', '\u0053\u0326'],
  ['\uac00', // 가 : Hangul
    '\uac00', '\u1100\u1161',
    '\uac00', '\u1100\u1161'],
  ['\uff76', // ｶ : Narrow Kana
    '\uff76', '\uff76',
    '\u30ab', '\u30ab'],
  ['\u00bc', // ¼ : Fractions
    '\u00bc', '\u00bc',
    '\u0031\u2044\u0034', '\u0031\u2044\u0034'],
  ['\u01c6', // ǆ  : Latin ligature
    '\u01c6', '\u01c6',
    '\u0064\u017e', '\u0064\u007a\u030c'],
  ['s\u0307\u0323', // s + dot above + dot below, ordering of combining marks
    '\u1e69', 's\u0323\u0307',
    '\u1e69', 's\u0323\u0307'],
  ['\u3300', // ㌀ : Squared characters
    '\u3300', '\u3300',
    '\u30a2\u30d1\u30fc\u30c8', // アパート
    '\u30a2\u30cf\u309a\u30fc\u30c8'], // アパート
  ['\ufe37', // ︷ : Vertical forms
    '\ufe37', '\ufe37',
    '{' , '{'],
  ['\u2079', // ⁹ : superscript 9
    '\u2079', '\u2079',
    '9', '9'],
  ['\ufee5\ufee6\ufee7\ufee8', // Arabic forms
    '\ufee5\ufee6\ufee7\ufee8', '\ufee5\ufee6\ufee7\ufee8',
    '\u0646\u0646\u0646\u0646', '\u0646\u0646\u0646\u0646'],
  ['\u2460', // ① : Circled
    '\u2460', '\u2460',
    '1', '1'],
  ['\u210c', // ℌ : Font variants
    '\u210c', '\u210c',
    'H', 'H'],
  ['\u2126', // Ω : Singleton, OHM sign vs. Greek capital letter OMEGA
    '\u03a9', '\u03a9',
    '\u03a9', '\u03a9'],
  ['\ufdfb', // Long ligature, ARABIC LIGATURE JALLAJALALOUHOU
    '\ufdfb', '\ufdfb',
    '\u062C\u0644\u0020\u062C\u0644\u0627\u0644\u0647',
    '\u062C\u0644\u0020\u062C\u0644\u0627\u0644\u0647']
];

var testArray = function() {
  var kNFC = 1, kNFD = 2, kNFKC = 3, kNFKD = 4;
  for (var i = 0; i < testData.length; ++i) {
    // the original, NFC and NFD should normalize to the same thing
    for (var column = 0; column < 3; ++column) {
      var str = testData[i][column];
      assertEquals(str.normalize(), testData[i][kNFC]); // defaults to NFC
      assertEquals(str.normalize('NFC'), testData[i][kNFC]);
      assertEquals(str.normalize('NFD'), testData[i][kNFD]);
      assertEquals(str.normalize('NFKC'), testData[i][kNFKC]);
      assertEquals(str.normalize('NFKD'), testData[i][kNFKD]);
    }
  }
}();
