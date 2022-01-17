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

var services = [
  Intl.Collator,
  Intl.DateTimeFormat,
  Intl.NumberFormat,
  Intl.ListFormat,
  Intl.PluralRules,
  Intl.RelativeTimeFormat,
  Intl.Segmenter,
  Intl.v8BreakIterator,
];

for (const service of services) {
  let undef = service.supportedLocalesOf();
  assertEquals([], undef);

  let empty = service.supportedLocalesOf([]);
  assertEquals([], empty);

  let strLocale = service.supportedLocalesOf("sr");
  assertEquals("sr", strLocale[0]);

  var locales = ["sr-Thai-RS", "de", "zh-CN"];
  let multiLocale = service.supportedLocalesOf(
      locales, {localeMatcher: "lookup"});
  assertEquals("sr-Thai-RS", multiLocale[0]);
  assertEquals("de", multiLocale[1]);
  assertEquals("zh-CN", multiLocale[2]);

  let numLocale = service.supportedLocalesOf(1);
  assertEquals([], numLocale);
  assertThrows(function() {
    numLocale = Intl.Collator.supportedLocalesOf([1]);
  }, TypeError);

  extensionLocale = service.supportedLocalesOf("id-u-co-pinyin");
  assertEquals("id-u-co-pinyin", extensionLocale[0]);

  bestFitLocale = service.supportedLocalesOf("de", {
    localeMatcher: "best fit"
  });
  assertEquals("de", bestFitLocale[0]);

  // Need a better test for "lookup" once it differs from "best fit".
  lookupLocale = service.supportedLocalesOf("zh-CN", {
    localeMatcher: "lookup"
  });
  assertEquals("zh-CN", lookupLocale[0]);

  assertThrows(function() {
    service.supportedLocalesOf("id-u-co-pinyin", { localeMatcher: "xyz" });
  }, RangeError);

  privateuseLocale = service.supportedLocalesOf("en-US-x-twain");
  assertEquals("en-US-x-twain", privateuseLocale[0]);

  assertThrows(() => service.supportedLocalesOf("x-twain"), RangeError);


  if (service != Intl.PluralRules) {
    grandfatheredLocale = service.supportedLocalesOf("art-lojban");
    assertEquals(undefined, grandfatheredLocale[0]);
  }

  assertThrows(() => service.supportedLocalesOf("x-pwn"), RangeError);

  unicodeInPrivateuseLocale = service.supportedLocalesOf(
    "en-US-x-u-co-phonebk"
  );
  assertEquals("en-US-x-u-co-phonebk", unicodeInPrivateuseLocale[0]);
}
