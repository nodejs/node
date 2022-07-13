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

// Tests String.prototype.localeCompare method override.

var testData = {
  'en': ['blood', 'bull', 'ascend', 'zed', 'down'],
  'sr': ['новине', 'ограда', 'жирафа', 'Никола', 'Андрија', 'Стара Планина',
         'џак', 'алав', 'ћук', 'чука'],
  'de': ['März', 'Fuße', 'FUSSE', 'Fluße', 'Flusse', 'flusse', 'fluße',
         'flüße', 'flüsse']
};


function testArrays(locale) {
  var data;
  if (locale === undefined) {
    data = testData['en'];
    locale = [];
  } else {
    data = testData[locale];
  }

  var collator = new Intl.Collator(locale, options);
  var collatorResult = data.sort(collator.compare);
  var localeCompareResult = data.sort(function(a, b) {
    return a.localeCompare(b, locale, options)
  });
  assertEquals(collatorResult, localeCompareResult);
}


// Defaults
var options = undefined;
testArrays();


// Specify locale, keep default options.
options = undefined;
Object.keys(testData).forEach(testArrays);


// Specify locale and options.
options = {caseFirst: 'upper'};
Object.keys(testData).forEach(testArrays);
