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

// Make sure normalization is always on, and normalization flag is ignored.

// We need a character with two combining marks, from two different classes,
// to make ICU fail comparison without normalization (upper, lower accent).
// We will just switch order of combining characters to try to induce failure.

// FYI, this one wouldn't work, since both accents are from the same class:
// http://unicode.org/cldr/utility/character.jsp?a=01DF

// See http://demo.icu-project.org/icu-bin/nbrowser?t=&s=1E09&uv=0 and
// http://unicode.org/cldr/utility/character.jsp?a=1E09 for character details.
var toCompare = ['\u0063\u0327\u0301', '\u0063\u0301\u0327'];

// Try with normalization off (as an option).
var collator = Intl.Collator([], {normalization: false});
// If we accepted normalization parameter, this would have failed.
assertEquals(0, collator.compare(toCompare[0], toCompare[1]));
assertFalse(collator.resolvedOptions().hasOwnProperty('normalization'));

// Try with normalization off (as Unicode extension).
collator = Intl.Collator(['de-u-kk-false']);
// If we accepted normalization parameter, this would have failed.
assertEquals(0, collator.compare(toCompare[0], toCompare[1]));
assertFalse(collator.resolvedOptions().hasOwnProperty('normalization'));

// Normalization is on by default.
collator = Intl.Collator();
assertEquals(0, collator.compare(toCompare[0], toCompare[1]));
assertFalse(collator.resolvedOptions().hasOwnProperty('normalization'));
