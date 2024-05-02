// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Well-formed but invalid calendar should not throw RangeError.
var calendar = "abc";
var len = 3;
var expected =  new Intl.DateTimeFormat("en").resolvedOptions().calendar;
var df;

for (var i = 3; i < 20; i++, len++, calendar += "a") {
  assertDoesNotThrow(() => df = new Intl.DateTimeFormat("en", {calendar}),
      "Well-formed calendar should not throw");
  assertEquals(expected, df.resolvedOptions().calendar);
  if (len == 8) {
    calendar += "-ab";
    len = 2;
  }
}
