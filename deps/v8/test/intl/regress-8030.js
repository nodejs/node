// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-relative-time-format

var locales = ["tlh", "id", "en"];
var referenceRelativeTimeFormat = new Intl.RelativeTimeFormat(locales);
var referenceFormatted = referenceRelativeTimeFormat.format(3, "day");

class MyFormat extends Intl.RelativeTimeFormat {
  constructor(locales, options) {
    super(locales, options);
    // could initialize MyRelativeTimeFormat properties
  }
  // could add methods to MyRelativeTimeFormat.prototype
}

var format = new MyFormat(locales);
var actual = format.format(3, "day");
assertEquals(actual, referenceFormatted);
