// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-add-calendar-numbering-system

// Well-formed but invalid numberingSystem should not throw RangeError.
var numberingSystem = "abc";
var len = 3;

const intlClasses = [
    Intl.DateTimeFormat,
    Intl.NumberFormat,
    Intl.RelativeTimeFormat
];

intlClasses.forEach(function(cls) {
  var expected =  new cls("en").resolvedOptions().numberingSystem;
  var obj;
  for (var i = 3; i < 20; i++, len++, numberingSystem += "a") {
    assertDoesNotThrow(() => obj = new cls("en", {numberingSystem}),
        "Well-formed numberingSystem should not throw");
    assertEquals(expected, obj.resolvedOptions().numberingSystem);
    if (len == 8) {
      numberingSystem += "-ab";
      len = 2;
    }
  }
});
