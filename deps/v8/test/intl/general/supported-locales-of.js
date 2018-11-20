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

// Tests supportedLocalesOf method.

var undef = Intl.DateTimeFormat.supportedLocalesOf();
assertEquals([], undef);

var empty = Intl.DateTimeFormat.supportedLocalesOf([]);
assertEquals([], empty);

var strLocale = Intl.DateTimeFormat.supportedLocalesOf('sr');
assertEquals('sr', strLocale[0]);

var multiLocale =
    Intl.DateTimeFormat.supportedLocalesOf(['sr-Thai-RS', 'de', 'zh-CN']);
assertEquals('sr-Thai-RS', multiLocale[0]);
assertEquals('de', multiLocale[1]);
assertEquals('zh-CN', multiLocale[2]);

collatorUndef = Intl.Collator.supportedLocalesOf();
assertEquals([], collatorUndef);

collatorEmpty = Intl.Collator.supportedLocalesOf([]);
assertEquals([], collatorEmpty);

collatorStrLocale = Intl.Collator.supportedLocalesOf('sr');
assertEquals('sr', collatorStrLocale[0]);

collatorMultiLocale =
    Intl.Collator.supportedLocalesOf(['sr-Thai-RS', 'de', 'zh-CN']);
assertEquals('sr-Thai-RS', collatorMultiLocale[0]);
assertEquals('de', collatorMultiLocale[1]);
assertEquals('zh-CN', collatorMultiLocale[2]);

numLocale = Intl.Collator.supportedLocalesOf(1);
assertEquals([], numLocale);

assertThrows(function() {
  numLocale = Intl.Collator.supportedLocalesOf([1]);
}, TypeError);

extensionLocale = Intl.Collator.supportedLocalesOf('id-u-co-pinyin');
assertEquals('id-u-co-pinyin', extensionLocale[0]);

bestFitLocale =
    Intl.Collator.supportedLocalesOf('de', {localeMatcher: 'best fit'});
assertEquals('de', bestFitLocale[0]);

// Need a better test for "lookup" once it differs from "best fit".
lookupLocale =
    Intl.Collator.supportedLocalesOf('zh-CN', {localeMatcher: 'lookup'});
assertEquals('zh-CN', lookupLocale[0]);

assertThrows(function() {
  Intl.Collator.supportedLocalesOf('id-u-co-pinyin', {localeMatcher: 'xyz'});
}, RangeError);
